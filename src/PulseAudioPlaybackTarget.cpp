/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifdef HAVE_LIBPULSE

#include "PulseAudioPlaybackTarget.h"
#include "ApplicationPlaybackSource.h"

#include <iostream>
#include <cmath>

using namespace std;

//#define DEBUG_PULSE_AUDIO_PLAYBACK_TARGET 1

namespace breakfastquay {

PulseAudioPlaybackTarget::PulseAudioPlaybackTarget(ApplicationPlaybackSource *source) :
    SystemPlaybackTarget(source),
    m_loop(0),
    m_api(0),
    m_context(0),
    m_out(0),
    m_bufferSize(0),
    m_sampleRate(0),
    m_paChannels(2),
    m_done(false),
    m_playbackReady(false),
    m_suspended(false)
{
#ifdef DEBUG_PULSE_AUDIO_PLAYBACK_TARGET
    cerr << "PulseAudioPlaybackTarget: Initialising for PulseAudio" << endl;
#endif

    m_name = source->getClientName();
    
    m_loop = pa_mainloop_new();
    if (!m_loop) {
        cerr << "ERROR: PulseAudioPlaybackTarget: Failed to create main loop" << endl;
        return;
    }

    m_api = pa_mainloop_get_api(m_loop);

    //!!! handle signals how?

    m_bufferSize = 20480;
    m_sampleRate = 44100;
    if (m_source && (m_source->getApplicationSampleRate() != 0)) {
	m_sampleRate = m_source->getApplicationSampleRate();
    }
    m_spec.rate = m_sampleRate;
    m_spec.channels = (uint8_t)m_paChannels;
    m_spec.format = PA_SAMPLE_FLOAT32NE;

    m_context = pa_context_new(m_api, m_name.c_str());
    if (!m_context) {
        cerr << "ERROR: PulseAudioPlaybackTarget: Failed to create context object" << endl;
        return;
    }

    pa_context_set_state_callback(m_context, contextStateChangedStatic, this);

    pa_context_connect(m_context, 0, (pa_context_flags_t)0, 0); // default server

    m_loopthread = thread([this]() { threadRun(); });

#ifdef DEBUG_PULSE_AUDIO_PLAYBACK_TARGET
    cerr << "PulseAudioPlaybackTarget: initialised OK" << endl;
#endif
}

PulseAudioPlaybackTarget::~PulseAudioPlaybackTarget()
{
    cerr << "PulseAudioPlaybackTarget::~PulseAudioPlaybackTarget()" << endl;

    m_done = true;

    lock_guard<recursive_mutex> guard(m_mutex);

    if (m_out) pa_stream_unref(m_out);

    if (m_context) pa_context_unref(m_context);

    if (m_loop) {
        pa_signal_done();
        pa_mainloop_quit(m_loop, 0);
        m_loopthread.join();
        pa_mainloop_free(m_loop);
    }
    
    cerr << "PulseAudioPlaybackTarget::~PulseAudioPlaybackTarget() done" << endl;
}

bool
PulseAudioPlaybackTarget::isTargetOK() const
{
    return (m_context != 0);
}

double
PulseAudioPlaybackTarget::getCurrentTime() const
{
    if (!m_out) return 0.0;
    
    pa_usec_t usec = 0;
    pa_stream_get_time(m_out, &usec);
    return double(usec) / 1000000.f;
}

void
PulseAudioPlaybackTarget::streamWriteStatic(pa_stream *,
                                            size_t length,
                                            void *data)
{
    PulseAudioPlaybackTarget *target = (PulseAudioPlaybackTarget *)data;
    if (length > INT_MAX) return;
    target->streamWrite(int(length));
}

void
PulseAudioPlaybackTarget::streamWrite(int requested)
{
#ifdef DEBUG_PULSE_AUDIO_PLAYBACK_TARGET    
    cout << "PulseAudioPlaybackTarget::streamWrite(" << requested << ")" << endl;
#endif
    if (m_done) return;

    // requested is a byte count for an interleaved buffer, so number
    // of frames = requested / (channels * sizeof(float))

    lock_guard<recursive_mutex> guard(m_mutex);

    pa_usec_t latency = 0;
    int negative = 0;
    if (!pa_stream_get_latency(m_out, &latency, &negative)) {
        int latframes = latencyFrames(latency);
        if (latframes > 0) m_source->setSystemPlaybackLatency(latframes);
    }

    static float *output = 0;
    static float **tmpbuf = 0;
    static int tmpbufch = 0;
    static int tmpbufsz = 0;

    int sourceChannels = m_source->getApplicationChannelCount();
    if (sourceChannels == 0) {
        //!!! but this is incredibly cpu-intensive and poor for
        //!!! battery life, because it happens all the time when not
        //!!! playing. should pause and resume the stream somehow
        int samples = int(requested / sizeof(float));
        output = new float[samples];
        for (int i = 0; i < samples; ++i) {
            output[i] = 0.f;
        }
        pa_stream_write(m_out, output, samples * sizeof(float),
                        0, 0, PA_SEEK_RELATIVE);
        m_source->setOutputLevels(0.f, 0.f);
        return;
    }

    int nframes = int(requested / (sourceChannels * sizeof(float))); //!!! this should be dividing by how many channels PA thinks it has!

    if (nframes > m_bufferSize) {
        cerr << "WARNING: PulseAudioPlaybackTarget::streamWrite: nframes " << nframes << " > m_bufferSize " << m_bufferSize << endl;
    }

#ifdef DEBUG_PULSE_AUDIO_PLAYBACK_TARGET
    cerr << "PulseAudioPlaybackTarget::streamWrite: nframes = " << nframes << endl;
#endif

    if (!tmpbuf || tmpbufch != sourceChannels || int(tmpbufsz) < nframes) {

	if (tmpbuf) {
	    for (int i = 0; i < tmpbufch; ++i) {
		delete[] tmpbuf[i];
	    }
	    delete[] tmpbuf;
	}

        if (output) {
            delete[] output;
        }

	tmpbufch = sourceChannels;
	tmpbufsz = nframes;
	tmpbuf = new float *[tmpbufch];

	for (int i = 0; i < tmpbufch; ++i) {
	    tmpbuf[i] = new float[tmpbufsz];
	}

        output = new float[tmpbufsz * m_paChannels];
    }
	
    int received = m_source->getSourceSamples(nframes, tmpbuf);

#ifdef DEBUG_PULSE_AUDIO_PLAYBACK_TARGET
    cerr << "received = " << received << endl;
#endif
    
    float peakLeft = 0.0, peakRight = 0.0;

    for (int ch = 0; ch < m_paChannels; ++ch) {
	
	float peak = 0.0;

	if (ch < sourceChannels) {

	    // PulseAudio samples are interleaved
	    for (int i = 0; i < nframes; ++i) {
                if (i < received) {
                    output[i * m_paChannels + ch] = tmpbuf[ch][i] * m_outputGain;
                    float sample = fabsf(output[i * m_paChannels + ch]);
                    if (sample > peak) peak = sample;
                } else {
                    output[i * m_paChannels + ch] = 0.f;
                }
	    }

	} else if (ch == 1 && sourceChannels == 1) {

	    for (int i = 0; i < nframes; ++i) {
                if (i < received) {
                    output[i * m_paChannels + ch] = tmpbuf[0][i] * m_outputGain;
                    float sample = fabsf(output[i * m_paChannels + ch]);
                    if (sample > peak) peak = sample;
                } else {
                    output[i * m_paChannels + ch] = 0.f;
                }
	    }

	} else {
	    for (int i = 0; i < nframes; ++i) {
		output[i * m_paChannels + ch] = 0;
	    }
	}

	if (ch == 0) peakLeft = peak;
	if (ch > 0 || sourceChannels == 1) peakRight = peak;
    }

#ifdef DEBUG_PULSE_AUDIO_PLAYBACK_TARGET
    cerr << "calling pa_stream_write with "
              << nframes * tmpbufch * sizeof(float) << " bytes" << endl;
#endif

    pa_stream_write(m_out, output, nframes * tmpbufch * sizeof(float),
                    0, 0, PA_SEEK_RELATIVE);

//    cerr << "peaks: " << peakLeft << ", " << peakRight << endl;

    m_source->setOutputLevels(peakLeft, peakRight);

    return;
}

void
PulseAudioPlaybackTarget::streamStateChangedStatic(pa_stream *stream,
                                                   void *data)
{
    PulseAudioPlaybackTarget *target = (PulseAudioPlaybackTarget *)data;
    target->streamStateChanged(stream);
}

void
PulseAudioPlaybackTarget::streamStateChanged(pa_stream *stream)
{
#ifdef DEBUG_PULSE_AUDIO_PLAYBACK_TARGET
    cerr << "PulseAudioPlaybackTarget::streamStateChanged" << endl;
#endif

    assert(stream == m_out);

    lock_guard<recursive_mutex> guard(m_mutex);

    switch (pa_stream_get_state(stream)) {

        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
            break;

        case PA_STREAM_READY:
        {
            m_playbackReady = true;

            pa_usec_t latency = 0;
            int negative = 0;
            if (pa_stream_get_latency(m_out, &latency, &negative)) {
                cerr << "PulseAudioPlaybackTarget::streamStateChanged: Failed to query latency" << endl;
            }
            cerr << "Latency = " << latency << " usec" << endl;
            int latframes = latencyFrames(latency);
            cerr << "that's " << latframes << " frames" << endl;

            const pa_buffer_attr *attr;
            if (!(attr = pa_stream_get_buffer_attr(m_out))) {
                cerr << "PulseAudioPlaybackTarget::streamStateChanged: Cannot query stream buffer attributes" << endl;
                m_source->setSystemPlaybackSampleRate(m_sampleRate);
                m_source->setSystemPlaybackLatency(latframes);
            } else {
                cerr << "PulseAudioPlaybackTarget::streamStateChanged: stream max length = " << attr->maxlength << endl;
                int latency = attr->tlength;
                cerr << "latency = " << latency << endl;
                m_source->setSystemPlaybackSampleRate(m_sampleRate);
                m_source->setSystemPlaybackLatency(latframes);
            }

            break;
        }

        case PA_STREAM_FAILED:
        default:
            cerr << "PulseAudioPlaybackTarget::streamStateChanged: Error: "
                      << pa_strerror(pa_context_errno(m_context)) << endl;
            //!!! do something...
            break;
    }
}

void
PulseAudioPlaybackTarget::suspend()
{
    lock_guard<recursive_mutex> guard(m_mutex);

    if (m_suspended) return;

    if (m_out) {
        pa_stream_cork(m_out, 1, 0, 0);
        pa_stream_flush(m_out, 0, 0);
    }

    m_suspended = true;
    
#ifdef DEBUG_PULSE_AUDIO_PLAYBACK_TARGET
    cerr << "corked!" << endl;
#endif
}

void
PulseAudioPlaybackTarget::resume()
{
    lock_guard<recursive_mutex> guard(m_mutex);

    if (!m_suspended) return;

    if (m_out) {
        pa_stream_cork(m_out, 0, 0, 0);
    }

    m_suspended = false;
    
#ifdef DEBUG_PULSE_AUDIO_PLAYBACK_TARGET
    cerr << "uncorked!" << endl;
#endif
}

void
PulseAudioPlaybackTarget::contextStateChangedStatic(pa_context *,
                                                    void *data)
{
    PulseAudioPlaybackTarget *target = (PulseAudioPlaybackTarget *)data;
    target->contextStateChanged();
}

void
PulseAudioPlaybackTarget::contextStateChanged()
{
#ifdef DEBUG_PULSE_AUDIO_PLAYBACK_TARGET
    cerr << "PulseAudioPlaybackTarget::contextStateChanged" << endl;
#endif
    lock_guard<recursive_mutex> guard(m_mutex);

    switch (pa_context_get_state(m_context)) {

        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY:
        {
            cerr << "PulseAudioPlaybackTarget::contextStateChanged: Ready"
                      << endl;

            m_out = pa_stream_new(m_context, "Playback", &m_spec, 0);
            assert(m_out); //!!!
            
            pa_stream_set_state_callback(m_out, streamStateChangedStatic, this);
            pa_stream_set_write_callback(m_out, streamWriteStatic, this);
            pa_stream_set_overflow_callback(m_out, streamOverflowStatic, this);
            pa_stream_set_underflow_callback(m_out, streamUnderflowStatic, this);

            if (pa_stream_connect_playback
                (m_out, 0, 0,
                 pa_stream_flags_t(PA_STREAM_INTERPOLATE_TIMING |
                                   PA_STREAM_AUTO_TIMING_UPDATE),
                 0, 0)) { //??? return value
                cerr << "PulseAudioPlaybackTarget: Failed to connect playback stream" << endl;
            }

            break;
        }

        case PA_CONTEXT_TERMINATED:
            cerr << "PulseAudioPlaybackTarget::contextStateChanged: Terminated" << endl;
            //!!! do something...
            break;

        case PA_CONTEXT_FAILED:
        default:
            cerr << "PulseAudioPlaybackTarget::contextStateChanged: Error: "
                      << pa_strerror(pa_context_errno(m_context)) << endl;
            //!!! do something...
            break;
    }
}

void
PulseAudioPlaybackTarget::streamOverflowStatic(pa_stream *, void *)
{
    cerr << "PulseAudioPlaybackTarget::streamOverflowStatic: Overflow!" << endl;
}

void
PulseAudioPlaybackTarget::streamUnderflowStatic(pa_stream *, void *)
{
    cerr << "PulseAudioPlaybackTarget::streamUnderflowStatic: Underflow!" << endl;
}


}

#endif /* HAVE_LIBPULSE */

