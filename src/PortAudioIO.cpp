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

#ifdef HAVE_PORTAUDIO

#include "PortAudioIO.h"
#include "ApplicationPlaybackSource.h"
#include "ApplicationRecordTarget.h"
#include "Gains.h"

#include "bqvec/VectorOps.h"
#include "bqvec/Allocators.h"

#include <iostream>
#include <cassert>
#include <cmath>
#include <climits>

#ifndef __LINUX__
#ifndef _WIN32
#include <pthread.h>
#endif
#endif

//#define DEBUG_AUDIO_PORT_AUDIO_IO 1

using namespace std;

namespace breakfastquay {

#ifdef __LINUX__
extern "C" {
void
PaAlsa_EnableRealtimeScheduling(PaStream *, int);
}
#endif

static bool // true if "can attempt on this platform", not "succeeded"
enableRT(PaStream *
#ifdef __LINUX__
         stream
#endif
    ) {
#ifdef __LINUX__
    // This will link only if the PA ALSA host API is linked statically
    PaAlsa_EnableRealtimeScheduling(stream, 1);
    return true;
#else
    return false;
#endif
}

static bool // true if "can attempt on this platform", not "succeeded"
enableRT() { // on current thread
#ifndef __LINUX__
#ifndef _WIN32
    sched_param param;
    param.sched_priority = 20;
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &param)) {
        cerr << "PortAudioIO: NOTE: couldn't set RT scheduling class" << endl;
    } else {
        cerr << "PortAudioIO: NOTE: successfully set RT scheduling class" << endl;
    }
    return true;
#endif
#endif
    return false;
}

PortAudioIO::PortAudioIO(Mode mode,
                         ApplicationRecordTarget *target,
                         ApplicationPlaybackSource *source) :
    SystemAudioIO(target, source),
    m_stream(0),
    m_mode(mode),
    m_bufferSize(0),
    m_sampleRate(0),
    m_inputLatency(0),
    m_outputLatency(0),
    m_prioritySet(false),
    m_suspended(false),
    m_buffers(0),
    m_bufferChannels(0)
{
    if (m_mode == Mode::Playback) {
        m_target = 0;
    }
    if (m_mode == Mode::Record) {
        m_source = 0;
    }
    
    PaError err;

    err = Pa_Initialize();
    if (err != paNoError) {
	cerr << "ERROR: PortAudioIO: Failed to initialize PortAudio" << endl;
	return;
    }

    m_bufferSize = 0;

    PaStreamParameters ip, op;
    ip.device = Pa_GetDefaultInputDevice();
    op.device = Pa_GetDefaultOutputDevice();

    const PaDeviceInfo *outInfo = Pa_GetDeviceInfo(op.device);
    m_sampleRate = outInfo->defaultSampleRate;

    m_inputChannels = 2;
    m_outputChannels = 2;

    int sourceRate = 0;
    int targetRate = 0;
    
    if (m_source) {
        sourceRate = m_source->getApplicationSampleRate();
        if (sourceRate != 0) {
            m_sampleRate = sourceRate;
        }
        if (m_source->getApplicationChannelCount() != 0) {
            m_outputChannels = m_source->getApplicationChannelCount();
        }
    }
    if (m_target) {
        targetRate = m_target->getApplicationSampleRate();
        if (targetRate != 0) {
            if (sourceRate != 0 && sourceRate != targetRate) {
                cerr << "WARNING: PortAudioIO: Source and target both provide sample rates, but different ones (source " << sourceRate << ", target " << targetRate << ") - using source rate" << endl;
            } else {
                m_sampleRate = targetRate;
            }
        }
        if (m_target->getApplicationChannelCount() != 0) {
            m_inputChannels = m_target->getApplicationChannelCount();
        }
    }
    if (m_sampleRate == 0) {
        m_sampleRate = 44100;
    }

    ip.channelCount = m_inputChannels;
    op.channelCount = m_outputChannels;
    ip.sampleFormat = paFloat32;
    op.sampleFormat = paFloat32;
    ip.suggestedLatency = 0.2;
    op.suggestedLatency = 0.2;
    ip.hostApiSpecificStreamInfo = 0;
    op.hostApiSpecificStreamInfo = 0;

    m_bufferSize = 0;
    err = openStream(m_mode, &m_stream, &ip, &op, m_sampleRate,
                     paFramesPerBufferUnspecified, this);

    if (err != paNoError) {
	m_bufferSize = 1024;
        err = openStream(m_mode, &m_stream, &ip, &op, m_sampleRate,
                         1024, this);
    }

    if (err != paNoError) {
        if (m_inputChannels != 2 || m_outputChannels != 2) {

            cerr << "WARNING: PortAudioIO: Failed to open PortAudio stream: "
                 << Pa_GetErrorText(err) << ": trying again with 2x2 configuration"
                 << endl;
            
            m_inputChannels = 2;
            m_outputChannels = 2;
            ip.channelCount = m_inputChannels;
            op.channelCount = m_outputChannels;

            m_bufferSize = 0;
            err = openStream(m_mode, &m_stream, &ip, &op, m_sampleRate,
                             paFramesPerBufferUnspecified, this);

            m_bufferSize = 1024;
            if (err != paNoError) {
                err = openStream(m_mode, &m_stream, &ip, &op, m_sampleRate,
                                 1024, this);
            }
        }
    }
    
    if (err != paNoError) {
	cerr << "ERROR: PortAudioIO: Failed to open PortAudio stream: "
             << Pa_GetErrorText(err) << endl;
	m_stream = 0;
	Pa_Terminate();
	return;
    }

    const PaStreamInfo *info = Pa_GetStreamInfo(m_stream);
    m_outputLatency = int(info->outputLatency * m_sampleRate + 0.001);
    m_inputLatency = int(info->inputLatency * m_sampleRate + 0.001);
    if (m_bufferSize == 0) m_bufferSize = m_outputLatency;
    if (m_bufferSize == 0) m_bufferSize = m_inputLatency;

    if (enableRT(m_stream)) {
        m_prioritySet = true;
    }

    err = Pa_StartStream(m_stream);

    if (err != paNoError) {
	cerr << "ERROR: PortAudioIO: Failed to start PortAudio stream" << endl;
	Pa_CloseStream(m_stream);
	m_stream = 0;
	Pa_Terminate();
	return;
    }

    cerr << "PortAudioIO: block size " << m_bufferSize << endl;

    if (m_source) {
	m_source->setSystemPlaybackBlockSize(m_bufferSize);
	m_source->setSystemPlaybackSampleRate(int(round(m_sampleRate)));
	m_source->setSystemPlaybackLatency(m_outputLatency);
        m_source->setSystemPlaybackChannelCount(m_outputChannels);
    }

    if (m_target) {
	m_target->setSystemRecordBlockSize(m_bufferSize);
	m_target->setSystemRecordSampleRate(int(round(m_sampleRate)));
	m_target->setSystemRecordLatency(m_inputLatency);
        m_target->setSystemRecordChannelCount(m_inputChannels);
    }

    m_bufferChannels = std::max(m_inputChannels, m_outputChannels);
    m_buffers = allocate_and_zero_channels<float>(m_bufferChannels, m_bufferSize);
}

PortAudioIO::~PortAudioIO()
{
    if (m_stream) {
	PaError err;
        err = Pa_AbortStream(m_stream);
	if (err != paNoError) {
	    cerr << "ERROR: PortAudioIO: Failed to abort PortAudio stream" << endl;
	}
	err = Pa_CloseStream(m_stream);
	if (err != paNoError) {
	    cerr << "ERROR: PortAudioIO: Failed to close PortAudio stream" << endl;
	}
	Pa_Terminate();
    }

    deallocate_channels(m_buffers, m_bufferChannels);
}

PaError
PortAudioIO::openStream(Mode mode,
                        PaStream **stream,
                        const PaStreamParameters *inputParameters,
                        const PaStreamParameters *outputParameters,
                        double sampleRate,
                        unsigned long framesPerBuffer,
                        void *data)
{
    switch (mode) {
    case Mode::Playback:
        return Pa_OpenStream(stream, 0, outputParameters, sampleRate,
                             framesPerBuffer, paNoFlag, processStatic, data);
    case Mode::Record:
        return Pa_OpenStream(stream, inputParameters, 0, sampleRate,
                             framesPerBuffer, paNoFlag, processStatic, data);
    case Mode::Duplex:
        return Pa_OpenStream(stream, inputParameters, outputParameters,
                             sampleRate,
                             framesPerBuffer, paNoFlag, processStatic, data);
    };
    return paNoError;
}

bool
PortAudioIO::isSourceOK() const
{
    return (m_stream != 0 && m_mode != Mode::Playback);
}

bool
PortAudioIO::isTargetOK() const
{
    return (m_stream != 0 && m_mode != Mode::Record);
}

double
PortAudioIO::getCurrentTime() const
{
    if (!m_stream) return 0.0;
    else return Pa_GetStreamTime(m_stream);
}

void
PortAudioIO::suspend()
{
#ifdef DEBUG_AUDIO_PORT_AUDIO_IO
    cerr << "PortAudioIO::suspend called" << endl;
#endif

    if (m_suspended || !m_stream) return;
    PaError err = Pa_StopStream(m_stream);
    if (err != paNoError) {
        cerr << "ERROR: PortAudioIO: Failed to abort PortAudio stream" << endl;
    }
    m_suspended = true;
#ifdef DEBUG_AUDIO_PORT_AUDIO_IO
    cerr << "suspended" << endl;
#endif
}

void
PortAudioIO::resume()
{
#ifdef DEBUG_AUDIO_PORT_AUDIO_IO
    cerr << "PortAudioIO::resume called" << endl;
#endif

    if (!m_suspended || !m_stream) return;
    PaError err = Pa_StartStream(m_stream);
    if (err != paNoError) {
        cerr << "ERROR: PortAudioIO: Failed to restart PortAudio stream" << endl;
    }
    m_suspended = false;
#ifdef DEBUG_AUDIO_PORT_AUDIO_IO
    cerr << "resumed" << endl;
#endif
}

int
PortAudioIO::processStatic(const void *input, void *output,
                           unsigned long nframes,
                           const PaStreamCallbackTimeInfo *timeInfo,
                           PaStreamCallbackFlags flags, void *data)
{
    return ((PortAudioIO *)data)->process(input, output,
                                          nframes, timeInfo,
                                          flags);
}

int
PortAudioIO::process(const void *inputBuffer, void *outputBuffer,
                     unsigned long pa_nframes,
                     const PaStreamCallbackTimeInfo *,
                     PaStreamCallbackFlags)
{
#ifdef DEBUG_AUDIO_PORT_AUDIO_IO    
    cout << "PortAudioIO::process(" << pa_nframes << ")" << endl;
#endif

    if (!m_prioritySet) {
        enableRT();
        m_prioritySet = true;
    }

    if (!m_source && !m_target) return 0;

    if (pa_nframes > INT_MAX) pa_nframes = 0;

    int nframes = int(pa_nframes);

    int sourceChannels = m_source ? m_source->getApplicationChannelCount() : 0;
    int targetChannels = m_target ? m_target->getApplicationChannelCount() : 0;

    if (nframes > m_bufferSize ||
        sourceChannels > m_bufferChannels ||
        targetChannels > m_bufferChannels) {
        int bufferChannels = std::max(sourceChannels, targetChannels);
        m_buffers = reallocate_and_zero_extend_channels
            (m_buffers,
             m_bufferChannels, m_bufferSize,
             bufferChannels, nframes);
        m_bufferChannels = bufferChannels;
        m_bufferSize = nframes;
    }
    
    float *input = (float *)inputBuffer;
    float *output = (float *)outputBuffer;

    float peakLeft, peakRight;

    if (m_target) {

        v_deinterleave
            (m_buffers, input, m_inputChannels, nframes);
        
        v_reconfigure_channels_inplace
            (m_buffers, targetChannels, m_inputChannels, nframes);

        peakLeft = 0.0, peakRight = 0.0;

        for (int c = 0; c < targetChannels && c < 2; ++c) {
            float peak = 0.f;
            for (int i = 0; i < nframes; ++i) {
                if (m_buffers[c][i] > peak) {
                    peak = m_buffers[c][i];
                }
            }
            if (c == 0) peakLeft = peak;
            if (c > 0 || targetChannels == 1) peakRight = peak;
        }

        m_target->putSamples(nframes, m_buffers);
        m_target->setInputLevels(peakLeft, peakRight);
    }

    if (m_source) {

        int received = m_source->getSourceSamples(nframes, m_buffers);

        if (received < nframes) {
            for (int c = 0; c < sourceChannels; ++c) {
                v_zero(m_buffers[c] + received, nframes - received);
            }
        }

        v_reconfigure_channels_inplace
            (m_buffers, m_outputChannels, sourceChannels, nframes);

        auto gain = Gains::gainsFor(m_outputGain, m_outputBalance, m_outputChannels);
        for (int c = 0; c < m_outputChannels; ++c) {
            v_scale(m_buffers[c], gain[c], nframes);
        }

        peakLeft = 0.0, peakRight = 0.0;
        for (int c = 0; c < m_outputChannels && c < 2; ++c) {
            float peak = 0.f;
            for (int i = 0; i < nframes; ++i) {
                if (m_buffers[c][i] > peak) {
                    peak = m_buffers[c][i];
                }
            }
            if (c == 0) peakLeft = peak;
            if (c == 1 || m_outputChannels == 1) peakRight = peak;
        }

        v_interleave
            (output, m_buffers, m_outputChannels, nframes);
        
        m_source->setOutputLevels(peakLeft, peakRight);

    } else if (m_outputChannels > 0) {

        v_zero(output, m_outputChannels * nframes);
    }

    return 0;
}

}

#endif /* HAVE_PORTAUDIO */

