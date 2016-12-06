/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    bqaudioio

    Copyright 2007-2016 Particular Programs Ltd.

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Except as contained in this notice, the names of Chris Cannam and
    Particular Programs Ltd shall not be used in advertising or
    otherwise to promote the sale, use or other dealings in this
    Software without prior written authorization.
*/

#ifdef HAVE_LIBPULSE

#include "PulseAudioIO.h"
#include "ApplicationPlaybackSource.h"
#include "ApplicationRecordTarget.h"
#include "Gains.h"

#include "bqvec/VectorOps.h"
#include "bqvec/Allocators.h"

#include <iostream>
#include <cmath>
#include <climits>

using namespace std;

//#define DEBUG_PULSE_AUDIO_IO 1

namespace breakfastquay {

PulseAudioIO::PulseAudioIO(Mode mode,
                           ApplicationRecordTarget *target,
                           ApplicationPlaybackSource *source) :
    SystemAudioIO(target, source),
    m_mode(mode),
    m_loop(0),
    m_api(0),
    m_context(0),
    m_in(0), 
    m_out(0),
    m_buffers(0),
    m_interleaved(0),
    m_bufferChannels(0),
    m_bufferSize(0),
    m_sampleRate(0),
    m_done(false),
    m_captureReady(false),
    m_playbackReady(false),
    m_suspended(false)
{
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO: Initialising for PulseAudio" << endl;
#endif
    
    if (m_mode == Mode::Playback) {
        m_target = 0;
    }
    if (m_mode == Mode::Record) {
        m_source = 0;
    }

    m_name = (source ? source->getClientName() :
              target ? target->getClientName() : "bqaudioio");

    m_loop = pa_mainloop_new();
    if (!m_loop) {
        cerr << "ERROR: PulseAudioIO: Failed to create main loop" << endl;
        return;
    }

    m_api = pa_mainloop_get_api(m_loop);

    int sourceRate = 0;
    int targetRate = 0;

    m_outSpec.channels = 2;
    if (m_source) {
        sourceRate = m_source->getApplicationSampleRate();
        if (sourceRate != 0) {
            m_sampleRate = sourceRate;
        }
        if (m_source->getApplicationChannelCount() != 0) {
            m_outSpec.channels = (uint8_t)m_source->getApplicationChannelCount();
        }
    }
    
    m_inSpec.channels = 2;
    if (m_target) {
        targetRate = m_target->getApplicationSampleRate();
        if (targetRate != 0) {
            if (sourceRate != 0 && sourceRate != targetRate) {
                cerr << "WARNING: PulseAudioIO: Source and target both provide sample rates, but different ones (source " << sourceRate << ", target " << targetRate << ") - using source rate" << endl;
            } else {
                m_sampleRate = targetRate;
            }
        }
        if (m_target->getApplicationChannelCount() != 0) {
            m_inSpec.channels = (uint8_t)m_target->getApplicationChannelCount();
        }
    }

    if (m_sampleRate == 0) {
        m_sampleRate = 44100;
    }

    m_bufferSize = m_sampleRate / 2; // initially
    
    m_inSpec.rate = m_sampleRate;
    m_outSpec.rate = m_sampleRate;
    
    m_inSpec.format = PA_SAMPLE_FLOAT32NE;
    m_outSpec.format = PA_SAMPLE_FLOAT32NE;
    
    m_bufferChannels = std::max(m_inSpec.channels, m_outSpec.channels);
    m_buffers = allocate_and_zero_channels<float>(m_bufferChannels, m_bufferSize);
    m_interleaved = allocate_and_zero<float>(m_bufferChannels * m_bufferSize);

    m_context = pa_context_new(m_api, m_name.c_str());
    if (!m_context) {
        cerr << "ERROR: PulseAudioIO: Failed to create context object" << endl;
        return;
    }

    pa_context_set_state_callback(m_context, contextStateChangedStatic, this);

    pa_context_connect(m_context, 0, (pa_context_flags_t)0, 0); // default server

    m_loopthread = thread([this]() { threadRun(); });

#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO: initialised OK" << endl;
#endif
}

PulseAudioIO::~PulseAudioIO()
{
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::~PulseAudioIO()" << endl;
#endif

    {
        if (m_loop) {
            pa_mainloop_wakeup(m_loop);
        }
        
        lock_guard<mutex> cguard(m_contextMutex);
        lock_guard<mutex> lguard(m_loopMutex);
        lock_guard<mutex> sguard(m_streamMutex);

        m_done = true;

        if (m_loop) {
            pa_signal_done();
            pa_mainloop_quit(m_loop, 0);
        }
    }
    
    m_loopthread.join();

    {
        lock_guard<mutex> sguard(m_streamMutex);
    
        if (m_in) {
            pa_stream_unref(m_in);
            m_in = 0;
        }
        if (m_out) {
            pa_stream_unref(m_out);
            m_out = 0;
        }
    }

    {
        lock_guard<mutex> cguard(m_contextMutex);
    
        if (m_context) {
            pa_context_unref(m_context);
            m_context = 0;
        }
    }

    deallocate_channels(m_buffers, m_bufferChannels);
    deallocate(m_interleaved);
    
    cerr << "PulseAudioIO::~PulseAudioIO() done" << endl;
}

void
PulseAudioIO::threadRun()
{
    int rv = 0;

    while (1) {

        {
#ifdef DEBUG_PULSE_AUDIO_IO
            cerr << "PulseAudioIO::threadRun: locking loop mutex for prepare" << endl;
#endif
            lock_guard<mutex> lguard(m_loopMutex);
            if (m_done) return;

            //!!! not nice
            rv = pa_mainloop_prepare(m_loop, 1000);
            if (rv < 0) {
                cerr << "ERROR: PulseAudioIO::threadRun: Failure in pa_mainloop_prepare" << endl;
                return;
            }
        }

        {
#ifdef DEBUG_PULSE_AUDIO_IO
            cerr << "PulseAudioIO::threadRun: locking loop mutex for poll" << endl;
#endif
            lock_guard<mutex> lguard(m_loopMutex);
            if (m_done) return;
            rv = pa_mainloop_poll(m_loop);
        }

        if (rv < 0) {
            cerr << "ERROR: PulseAudioIO::threadRun: Failure in pa_mainloop_poll" << endl;
            return;
        }

        {
#ifdef DEBUG_PULSE_AUDIO_IO
            cerr << "PulseAudioIO::threadRun: locking loop mutex for dispatch" << endl;
#endif
            lock_guard<mutex> lguard(m_loopMutex);
            if (m_done) return;

            rv = pa_mainloop_dispatch(m_loop);
            if (rv < 0) {
                cerr << "ERROR: PulseAudioIO::threadRun: Failure in pa_mainloop_dispatch" << endl;
                return;
            }
        }
    }
}

bool
PulseAudioIO::isSourceOK() const
{
    return (m_context != 0 && m_mode != Mode::Playback);
}

bool
PulseAudioIO::isTargetOK() const
{
    return (m_context != 0 && m_mode != Mode::Record);
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
    return double(usec) / 1000000.0;
}

void
PulseAudioIO::streamWriteStatic(pa_stream *,
                                size_t length,
                                void *data)
{
    PulseAudioIO *io = (PulseAudioIO *)data;

    if (length > INT_MAX) return;
    
    io->streamWrite(int(length));
}

void
PulseAudioIO::checkBufferCapacity(int channels, int nframes)
{
    if (nframes > m_bufferSize ||
        channels > m_bufferChannels) {

        m_buffers = reallocate_and_zero_extend_channels
            (m_buffers,
             m_bufferChannels, m_bufferSize,
             channels, nframes);

        m_interleaved = reallocate
            (m_interleaved,
             m_bufferChannels * m_bufferSize,
             channels * nframes);
        
        m_bufferChannels = channels;
        m_bufferSize = nframes;
    }
}

void
PulseAudioIO::streamWrite(int requested)
{
#ifdef DEBUG_PULSE_AUDIO_IO    
    cerr << "PulseAudioIO::streamWrite(" << requested << ")" << endl;
#endif

#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::streamWrite: locking stream mutex" << endl;
#endif

    // Pulse is a consumer system with long buffers, this is not a RT
    // context like the other drivers
    lock_guard<mutex> guard(m_streamMutex);
    if (m_done) return;
    if (!m_source) return;

    pa_usec_t latency = 0;
    int negative = 0;
    if (!pa_stream_get_latency(m_out, &latency, &negative)) {
        int latframes = latencyFrames(latency);
        if (latframes > 0) m_source->setSystemPlaybackLatency(latframes);
    }

    int channels = m_outSpec.channels;
    if (channels == 0) return;

    int nframes = requested / int(channels * sizeof(float));

    checkBufferCapacity(channels, nframes);

#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::streamWrite: nframes = " << nframes << endl;
#endif

    int received = m_source->getSourceSamples(nframes, m_buffers);
    
    if (received < nframes) {
        for (int c = 0; c < channels; ++c) {
            v_zero(m_buffers[c] + received, nframes - received);
        }
    }
        
    float peakLeft = 0.0, peakRight = 0.0;
    auto gain = Gains::gainsFor(m_outputGain, m_outputBalance, channels); 

    for (int c = 0; c < channels; ++c) {
        v_scale(m_buffers[c], gain[c], nframes);
    }
    
    for (int c = 0; c < channels && c < 2; ++c) {
	float peak = 0.f;
        for (int i = 0; i < nframes; ++i) {
            if (m_buffers[c][i] > peak) {
                peak = m_buffers[c][i];
            }
        }
        if (c == 0) peakLeft = peak;
        if (c == 1 || channels == 1) peakRight = peak;
    }

    v_interleave(m_interleaved, m_buffers, channels, nframes);

#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "calling pa_stream_write with "
         << nframes * channels * sizeof(float) << " bytes" << endl;
#endif

    pa_stream_write(m_out, m_interleaved,
                    nframes * channels * sizeof(float),
                    0, 0, PA_SEEK_RELATIVE);

    m_source->setOutputLevels(peakLeft, peakRight);

    return;
}

void
PulseAudioIO::streamReadStatic(pa_stream *,
                               size_t length,
                               void *data)
{
    PulseAudioIO *io = (PulseAudioIO *)data;
    
    if (length > INT_MAX) return;
    
    io->streamRead(int(length));
}

void
PulseAudioIO::streamRead(int available)
{
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::streamRead(" << available << ")" << endl;
#endif

#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::streamRead: locking stream mutex" << endl;
#endif
    lock_guard<mutex> guard(m_streamMutex);
    if (m_done) return;
    if (!m_target) return;
    
    pa_usec_t latency = 0;
    int negative = 0;
    if (!pa_stream_get_latency(m_in, &latency, &negative)) {
        int latframes = latencyFrames(latency);
        if (latframes > 0) m_target->setSystemRecordLatency(latframes);
    }

    int channels = m_inSpec.channels;
    if (channels == 0) return;

    int nframes = available / int(channels * sizeof(float));

#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::streamRead: nframes = " << nframes << endl;
#endif

    checkBufferCapacity(channels, nframes);
    
    float peakLeft = 0.0, peakRight = 0.0;

    size_t actual = available;
    
    const void *input = 0;
    pa_stream_peek(m_in, &input, &actual);

    int actualFrames = int(actual) / int(channels * sizeof(float));

    if (actualFrames < nframes) {
        cerr << "WARNING: streamRead: read " << actualFrames << " frames, expected " << nframes << endl;
    }
    
    const float *finput = (const float *)input;

    v_deinterleave(m_buffers, finput, channels, actualFrames);

    for (int c = 0; c < channels && c < 2; ++c) {
	float peak = 0.f;
        for (int i = 0; i < actualFrames; ++i) {
            if (m_buffers[c][i] > peak) {
                peak = m_buffers[c][i];
            }
        }
	if (c == 0) peakLeft = peak;
	if (c > 0 || channels == 1) peakRight = peak;
    }

    m_target->putSamples(actualFrames, m_buffers);
    m_target->setInputLevels(peakLeft, peakRight);

    pa_stream_drop(m_in);

    return;
}

void
PulseAudioIO::streamStateChangedStatic(pa_stream *stream,
                                            void *data)
{
    PulseAudioIO *io = (PulseAudioIO *)data;
    
    io->streamStateChanged(stream);
}

void
PulseAudioIO::streamStateChanged(pa_stream *stream)
{
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::streamStateChanged" << endl;
#endif

#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::streamStateChanged: locking stream mutex" << endl;
#endif
    lock_guard<mutex> guard(m_streamMutex);
    if (m_done) return;

    assert(stream == m_in || stream == m_out);

    switch (pa_stream_get_state(stream)) {

        case PA_STREAM_UNCONNECTED:
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
            
            if (m_source && (stream == m_out)) {
                m_source->setSystemPlaybackSampleRate(m_sampleRate);
                m_source->setSystemPlaybackChannelCount(m_outSpec.channels);
                if (pa_stream_get_latency(m_out, &latency, &negative)) {
                    cerr << "PulseAudioIO::streamStateChanged: Failed to query latency" << endl;
                } else {
                    cerr << "Latency = " << latency << " usec" << endl;
                    int latframes = latencyFrames(latency);
                    cerr << "that's " << latframes << " frames" << endl;
                    m_source->setSystemPlaybackLatency(latframes);
                }
            }
            if (m_target && (stream == m_in)) {
                m_target->setSystemRecordSampleRate(m_sampleRate);
                m_target->setSystemRecordChannelCount(m_inSpec.channels);
                if (pa_stream_get_latency(m_out, &latency, &negative)) {
                    cerr << "PulseAudioIO::streamStateChanged: Failed to query latency" << endl;
                } else {
                    cerr << "Latency = " << latency << " usec" << endl;
                    int latframes = latencyFrames(latency);
                    cerr << "that's " << latframes << " frames" << endl;
                    m_target->setSystemRecordLatency(latframes);
                }
            }

            break;
        }

        case PA_STREAM_FAILED:
        default:
            cerr << "PulseAudioIO::streamStateChanged: Error: "
                 << pa_strerror(pa_context_errno(m_context)) << endl;
            //!!! do something...
            break;
    }

#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::streamStateChanged complete" << endl;
#endif
}

void
PulseAudioIO::suspend()
{
    if (m_loop) pa_mainloop_wakeup(m_loop);
    
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::suspend: locking all mutexes" << endl;
#endif
    {
        lock_guard<mutex> cguard(m_contextMutex);
        if (m_suspended) return;
    }

    lock_guard<mutex> lguard(m_loopMutex);
    lock_guard<mutex> sguard(m_streamMutex);
    if (m_done) return;
    
    if (m_in) {
        pa_stream_cork(m_in, 1, 0, 0);
        pa_stream_flush(m_in, 0, 0);
    }

    if (m_out) {
        pa_stream_cork(m_out, 1, 0, 0);
        pa_stream_flush(m_out, 0, 0);
    }

    m_suspended = true;
    
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "corked!" << endl;
#endif
}

void
PulseAudioIO::resume()
{
    if (m_loop) pa_mainloop_wakeup(m_loop);
    
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::resume: locking all mutexes" << endl;
#endif
    {
        lock_guard<mutex> cguard(m_contextMutex);
        if (!m_suspended) return;
    }

    lock_guard<mutex> lguard(m_loopMutex);
    lock_guard<mutex> sguard(m_streamMutex);
    if (m_done) return;

    if (m_in) {
        pa_stream_flush(m_in, 0, 0);
        pa_stream_cork(m_in, 0, 0, 0);
    }

    if (m_out) {
        pa_stream_cork(m_out, 0, 0, 0);
    }

    m_suspended = false;
    
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "uncorked!" << endl;
#endif
}

void
PulseAudioIO::contextStateChangedStatic(pa_context *,
                                        void *data)
{
    PulseAudioIO *io = (PulseAudioIO *)data;
    io->contextStateChanged();
}

void
PulseAudioIO::contextStateChanged()
{
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::contextStateChanged" << endl;
#endif
#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::contextStateChanged: locking context mutex" << endl;
#endif
    lock_guard<mutex> guard(m_contextMutex);

    switch (pa_context_get_state(m_context)) {

        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY:
        {
            cerr << "PulseAudioIO::contextStateChanged: Ready"
                      << endl;

            m_in = pa_stream_new(m_context, "Capture", &m_inSpec, 0);
            if (!m_in) {
                cerr << "PulseAudioIO::contextStateChanged: Failed to create capture stream" << endl;
                return;
            }
            
            pa_stream_set_state_callback(m_in, streamStateChangedStatic, this);
            pa_stream_set_read_callback(m_in, streamReadStatic, this);
            pa_stream_set_overflow_callback(m_in, streamOverflowStatic, this);
            pa_stream_set_underflow_callback(m_in, streamUnderflowStatic, this);

            pa_stream_flags_t flags;

            flags = pa_stream_flags_t(PA_STREAM_INTERPOLATE_TIMING |
                                      PA_STREAM_AUTO_TIMING_UPDATE);
            if (m_suspended) {
                flags = pa_stream_flags_t(flags | PA_STREAM_START_CORKED);
            }
            
            if (pa_stream_connect_record (m_in, 0, 0, flags)) {
                cerr << "PulseAudioIO: Failed to connect record stream" << endl;
            }

            m_out = pa_stream_new(m_context, "Playback", &m_outSpec, 0);
            if (!m_out) {
                cerr << "PulseAudioIO::contextStateChanged: Failed to create playback stream" << endl;
                return;
            }
            
            pa_stream_set_state_callback(m_out, streamStateChangedStatic, this);
            pa_stream_set_write_callback(m_out, streamWriteStatic, this);
            pa_stream_set_overflow_callback(m_out, streamOverflowStatic, this);
            pa_stream_set_underflow_callback(m_out, streamUnderflowStatic, this);

            flags = pa_stream_flags_t(PA_STREAM_INTERPOLATE_TIMING |
                                      PA_STREAM_AUTO_TIMING_UPDATE);
            if (m_suspended) {
                flags = pa_stream_flags_t(flags | PA_STREAM_START_CORKED);
            }

            if (pa_stream_connect_playback(m_out, 0, 0, flags, 0, 0)) { 
                cerr << "PulseAudioIO: Failed to connect playback stream" << endl;
            }

            break;
        }

        case PA_CONTEXT_TERMINATED:
            cerr << "PulseAudioIO::contextStateChanged: Terminated" << endl;
            break;

        case PA_CONTEXT_FAILED:
        default:
            cerr << "PulseAudioIO::contextStateChanged: Error: "
                 << pa_strerror(pa_context_errno(m_context)) << endl;
            break;
    }

#ifdef DEBUG_PULSE_AUDIO_IO
    cerr << "PulseAudioIO::contextStateChanged complete" << endl;
#endif
}

void
PulseAudioIO::streamOverflowStatic(pa_stream *, void *data)
{
    cerr << "PulseAudioIO::streamOverflowStatic: Overflow!" << endl;

    PulseAudioIO *io = (PulseAudioIO *)data;

    if (io->m_target) io->m_target->audioProcessingOverload();
    if (io->m_source) io->m_source->audioProcessingOverload();
}

void
PulseAudioIO::streamUnderflowStatic(pa_stream *, void *data)
{
    cerr << "PulseAudioIO::streamUnderflowStatic: Underflow!" << endl;
    
    PulseAudioIO *io = (PulseAudioIO *)data;

    if (io->m_target) io->m_target->audioProcessingOverload();
    if (io->m_source) io->m_source->audioProcessingOverload();
}


}

#endif /* HAVE_LIBPULSE */

