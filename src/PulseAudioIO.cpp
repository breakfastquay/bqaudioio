/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifdef HAVE_LIBPULSE

#include "PulseAudioIO.h"
#include "ApplicationPlaybackSource.h"
#include "ApplicationRecordTarget.h"

#include "VectorOps.h"

#include <iostream>
#include <cmath>

using std::cerr;
using std::cout;
using std::endl;

//#define DEBUG_AUDIO_PULSE_AUDIO_IO 1

namespace Turbot {

PulseAudioIO::PulseAudioIO(ApplicationRecordTarget *target,
                           ApplicationPlaybackSource *source) :
    SystemAudioIO(target, source),
    m_mutex(QMutex::Recursive), //!!!???
    m_loop(0),
    m_api(0),
    m_context(0),
    m_in(0), 
    m_out(0),
    m_loopThread(0),
    m_bufferSize(0),
    m_sampleRate(0),
    m_paChannels(2),
    m_done(false),
    m_captureReady(false),
    m_playbackReady(false)
{
#ifdef DEBUG_AUDIO_PULSE_AUDIO_IO
    cerr << "PulseAudioIO: Initialising for PulseAudio" << endl;
#endif

    m_loop = pa_mainloop_new();
    if (!m_loop) {
        cerr << "ERROR: PulseAudioIO: Failed to create main loop" << endl;
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
    m_spec.channels = m_paChannels;
    m_spec.format = PA_SAMPLE_FLOAT32NE;

    m_context = pa_context_new(m_api, "turbot");
    if (!m_context) {
        cerr << "ERROR: PulseAudioIO: Failed to create context object" << endl;
        return;
    }

    pa_context_set_state_callback(m_context, contextStateChangedStatic, this);

    pa_context_connect(m_context, 0, (pa_context_flags_t)0, 0); // default server

    m_loopThread = new MainLoopThread(m_loop);
    m_loopThread->start();

#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO: initialised OK" << endl;
#endif
}

PulseAudioIO::~PulseAudioIO()
{
    cerr << "PulseAudioIO::~PulseAudioIO()" << endl;

//!!!    if (m_source) {
//        m_source->setTarget(0, m_bufferSize);
//    }

    m_done = true;

    QMutexLocker locker(&m_mutex);

    if (m_in) pa_stream_unref(m_in);
    if (m_out) pa_stream_unref(m_out);

    if (m_context) pa_context_unref(m_context);

    if (m_loop) {
        pa_signal_done();
        pa_mainloop_free(m_loop);
    }

    cerr << "PulseAudioIO::~PulseAudioIO() done" << endl;
}

bool
PulseAudioIO::isSourceOK() const
{
    return (m_context != 0);
}

bool
PulseAudioIO::isTargetOK() const
{
    return (m_context != 0);
}

bool
PulseAudioIO::isSourceReady() const
{
    return m_captureReady;
}

bool
PulseAudioIO::isTargetReady() const
{
    return m_playbackReady;
}

double
PulseAudioIO::getCurrentTime() const
{
    if (!m_out) return 0.0;
    
    pa_usec_t usec = 0;
    pa_stream_get_time(m_out, &usec);
    return usec / 1000000.f;
}

void
PulseAudioIO::streamWriteStatic(pa_stream *stream,
                                size_t length,
                                void *data)
{
    PulseAudioIO *target = (PulseAudioIO *)data;
    
    assert(stream == target->m_out);

    target->streamWrite(length);
}

void
PulseAudioIO::streamWrite(int requested)
{
#ifdef DEBUG_AUDIO_PULSE_AUDIO_IO    
    cout << "PulseAudioIO::streamWrite(" << requested << ")" << endl;
#endif
    if (m_done) return;

    QMutexLocker locker(&m_mutex);

    pa_usec_t latency = 0;
    int negative = 0;
    if (!pa_stream_get_latency(m_out, &latency, &negative)) {
        int latframes = (latency / 1000000.f) * float(m_sampleRate);
        if (latframes > 0) m_source->setSystemPlaybackLatency(latframes);
    }

    static float *output = 0;
    static float **tmpbuf = 0;
    static int tmpbufch = 0;
    static int tmpbufsz = 0;

    int sourceChannels = m_source->getApplicationChannelCount();
    if (sourceChannels == 0) {
        return;
    }

    int nframes = requested / (sourceChannels * sizeof(float)); //!!! this should be dividing by how many channels PA thinks it has!

    if (nframes > m_bufferSize) {
        cerr << "WARNING: PulseAudioIO::streamWrite: nframes " << nframes << " > m_bufferSize " << m_bufferSize << endl;
    }

#ifdef DEBUG_AUDIO_PULSE_AUDIO_IO
    cout << "PulseAudioIO::streamWrite: nframes = " << nframes << endl;
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
	
    m_source->getSourceSamples(nframes, tmpbuf);

    float peakLeft = 0.0, peakRight = 0.0;

    for (int ch = 0; ch < m_paChannels; ++ch) {
	
	float peak = 0.0;

	if (ch < sourceChannels) {

	    // PulseAudio samples are interleaved
	    for (int i = 0; i < nframes; ++i) {
                output[i * m_paChannels + ch] = tmpbuf[ch][i] * m_outputGain;
                float sample = fabsf(output[i * m_paChannels + ch]);
                if (sample > peak) peak = sample;
	    }

	} else if (ch == 1 && sourceChannels == 1) {

	    for (int i = 0; i < nframes; ++i) {
                output[i * m_paChannels + ch] = tmpbuf[0][i] * m_outputGain;
                float sample = fabsf(output[i * m_paChannels + ch]);
                if (sample > peak) peak = sample;
	    }

	} else {
	    for (int i = 0; i < nframes; ++i) {
		output[i * m_paChannels + ch] = 0;
	    }
	}

	if (ch == 0) peakLeft = peak;
	if (ch > 0 || sourceChannels == 1) peakRight = peak;
    }

#ifdef DEBUG_AUDIO_PULSE_AUDIO_IO
    cerr << "calling pa_stream_write with "
              << nframes * tmpbufch * sizeof(float) << " bytes" << endl;
#endif

    pa_stream_write(m_out, output, nframes * tmpbufch * sizeof(float),
                    0, 0, PA_SEEK_RELATIVE);

    m_source->setOutputLevels(peakLeft, peakRight);

    return;
}

void
PulseAudioIO::streamReadStatic(pa_stream *stream,
                               size_t length,
                               void *data)
{
    PulseAudioIO *target = (PulseAudioIO *)data;
    
    assert(stream == target->m_in);

    target->streamRead(length);
}

void
PulseAudioIO::streamRead(int available)
{
#ifdef DEBUG_AUDIO_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::streamRead(" << available << ")" << endl;
#endif

    QMutexLocker locker(&m_mutex);

    pa_usec_t latency = 0;
    int negative = 0;
    if (!pa_stream_get_latency(m_in, &latency, &negative)) {
        int latframes = (latency / 1000000.f) * float(m_sampleRate);
        if (latframes > 0) m_target->setSystemRecordLatency(latframes);
    }

    int sz = pa_stream_readable_size(m_in);

    static const void *input = 0;
    static float **tmpbuf = 0;
    static int tmpbufch = 0;
    static int tmpbufsz = 0;

    int targetChannels = m_target->getApplicationChannelCount();

    //!!! need to handle mismatches between our and PA's expectations!

    if (targetChannels < 2) targetChannels = 2;

    int nframes = available / (targetChannels * sizeof(float)); //!!! this should be dividing by how many channels PA thinks it has!

    if (nframes > m_bufferSize) {
        cerr << "WARNING: PulseAudioIO::streamRead: nframes " << nframes << " > m_bufferSize " << m_bufferSize << endl;
    }

#ifdef DEBUG_AUDIO_PULSE_AUDIO_IO
    cout << "PulseAudioIO::streamRead: nframes = " << nframes << endl;
#endif

    if (!tmpbuf || tmpbufch != targetChannels || int(tmpbufsz) < nframes) {

	if (tmpbuf) {
	    for (int i = 0; i < tmpbufch; ++i) {
		delete[] tmpbuf[i];
	    }
	    delete[] tmpbuf;
	}

	tmpbufch = targetChannels;
	tmpbufsz = nframes;
	tmpbuf = new float *[tmpbufch];

	for (int i = 0; i < tmpbufch; ++i) {
	    tmpbuf[i] = new float[tmpbufsz];
	}
    }
	
    float peakLeft = 0.0, peakRight = 0.0;

    size_t actual = available;

    pa_stream_peek(m_in, &input, &actual);

    int actualFrames = actual / (targetChannels * sizeof(float)); //!!! this should be dividing by how many channels PA thinks it has!

    if (actualFrames < nframes) {
        cerr << "WARNING: streamRead: read " << actualFrames << " frames, expected " << nframes << endl;
    }
    
    const float *finput = (const float *)input;

    for (int ch = 0; ch < targetChannels; ++ch) {
	
	float peak = 0.0;

        // PulseAudio samples are interleaved
        for (int i = 0; i < nframes; ++i) {
            tmpbuf[ch][i] = finput[i * 2 + ch];
            float sample = fabsf(finput[i * 2 + ch]);
            if (sample > peak) peak = sample;
        }

	if (ch == 0) peakLeft = peak;
	if (ch > 0 || targetChannels == 1) peakRight = peak;
    }

    m_target->putSamples(nframes, tmpbuf);
    m_target->setInputLevels(peakLeft, peakRight);

    pa_stream_drop(m_in);

    return;
}

void
PulseAudioIO::streamStateChangedStatic(pa_stream *stream,
                                            void *data)
{
    PulseAudioIO *target = (PulseAudioIO *)data;
    
    assert(stream == target->m_in || stream == target->m_out);

    target->streamStateChanged(stream);
}

void
PulseAudioIO::streamStateChanged(pa_stream *stream)
{
#ifdef DEBUG_AUDIO_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::streamStateChanged" << endl;
#endif

    assert(stream == m_in || stream == m_out);

    QMutexLocker locker(&m_mutex);

    switch (pa_stream_get_state(stream)) {

        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
            break;

        case PA_STREAM_READY:
        {
            if (stream == m_in) {
                cerr << "PulseAudioIO::streamStateChanged: Capture ready" << endl;
                m_captureReady = true;
            } else {
                cerr << "PulseAudioIO::streamStateChanged: Playback ready" << endl;
                m_playbackReady = true;
            }                

            pa_usec_t latency = 0;
            int negative = 0;
            if (pa_stream_get_latency(m_out, &latency, &negative)) {
                cerr << "PulseAudioIO::contextStateChanged: Failed to query latency" << endl;
            }
            cerr << "Latency = " << latency << " usec" << endl;
            int latframes = (latency / 1000000.f) * float(m_sampleRate);
            cerr << "that's " << latframes << " frames" << endl;

            const pa_buffer_attr *attr;
            if (!(attr = pa_stream_get_buffer_attr(m_out))) {
                cerr << "PulseAudioIO::streamStateChanged: Cannot query stream buffer attributes" << endl;
                m_source->setSystemPlaybackBlockSize(4096);
                m_source->setSystemPlaybackSampleRate(m_sampleRate);
                m_source->setSystemPlaybackLatency(latframes);
            } else {
                cerr << "PulseAudioIO::streamStateChanged: stream max length = " << attr->maxlength << endl;
                int latency = attr->tlength;
                cerr << "latency = " << latency << endl;
                m_source->setSystemPlaybackBlockSize(attr->maxlength);
                m_source->setSystemPlaybackSampleRate(m_sampleRate);
                m_source->setSystemPlaybackLatency(latframes);
            }

            cerr << "PulseAudioIO: setting system record sample rate to " << m_sampleRate << endl;
            m_target->setSystemRecordSampleRate(m_sampleRate);

            break;
        }

        case PA_STREAM_FAILED:
        default:
            cerr << "PulseAudioIO::streamStateChanged: Error: "
                      << pa_strerror(pa_context_errno(m_context)) << endl;
            //!!! do something...
            break;
    }
}

void
PulseAudioIO::contextStateChangedStatic(pa_context *context,
                                                 void *data)
{
    PulseAudioIO *target = (PulseAudioIO *)data;
    
    assert(context == target->m_context);

    target->contextStateChanged();
}

void
PulseAudioIO::contextStateChanged()
{
#ifdef DEBUG_AUDIO_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::contextStateChanged" << endl;
#endif
    QMutexLocker locker(&m_mutex);

    switch (pa_context_get_state(m_context)) {

        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY:
        {
            cerr << "PulseAudioIO::contextStateChanged: Ready"
                      << endl;

            m_in = pa_stream_new(m_context, "Turbot capture", &m_spec, 0);
            assert(m_in); //!!!
            
            pa_stream_set_state_callback(m_in, streamStateChangedStatic, this);
            pa_stream_set_read_callback(m_in, streamReadStatic, this);
            pa_stream_set_overflow_callback(m_in, streamOverflowStatic, this);
            pa_stream_set_underflow_callback(m_in, streamUnderflowStatic, this);

            if (pa_stream_connect_record
                (m_in, 0, 0,
                 pa_stream_flags_t(PA_STREAM_INTERPOLATE_TIMING |
                                   PA_STREAM_AUTO_TIMING_UPDATE))) { //??? return value
                cerr << "PulseAudioIO: Failed to connect record stream" << endl;
            }

            m_out = pa_stream_new(m_context, "Turbot playback", &m_spec, 0);
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
                cerr << "PulseAudioIO: Failed to connect playback stream" << endl;
            }

            break;
        }

        case PA_CONTEXT_TERMINATED:
            cerr << "PulseAudioIO::contextStateChanged: Terminated" << endl;
            //!!! do something...
            break;

        case PA_CONTEXT_FAILED:
        default:
            cerr << "PulseAudioIO::contextStateChanged: Error: "
                      << pa_strerror(pa_context_errno(m_context)) << endl;
            //!!! do something...
            break;
    }
}

void
PulseAudioIO::streamOverflowStatic(pa_stream *, void *)
{
    cerr << "PulseAudioIO::streamOverflowStatic: Overflow!" << endl;
}

void
PulseAudioIO::streamUnderflowStatic(pa_stream *, void *)
{
    cerr << "PulseAudioIO::streamUnderflowStatic: Underflow!" << endl;
}


}

#endif /* HAVE_LIBPULSE */
