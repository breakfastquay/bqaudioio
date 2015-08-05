/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifdef HAVE_PORTAUDIO

#include "PortAudioIO.h"
#include "ApplicationPlaybackSource.h"
#include "ApplicationRecordTarget.h"

#include "bqvec/VectorOps.h"

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

PortAudioIO::PortAudioIO(ApplicationRecordTarget *target,
				   ApplicationPlaybackSource *source) :
    SystemAudioIO(target, source),
    m_stream(0),
    m_bufferSize(0),
    m_sampleRate(0),
    m_inputLatency(0),
    m_outputLatency(0),
    m_prioritySet(false)
{
    PaError err;

    err = Pa_Initialize();
    if (err != paNoError) {
	cerr << "ERROR: PortAudioIO: Failed to initialize PortAudio" << endl;
	return;
    }

    m_bufferSize = 0;
    m_sampleRate = 44100;

    if (m_source && (m_source->getApplicationSampleRate() != 0)) {
	m_sampleRate = m_source->getApplicationSampleRate();
    } else if (m_target && (m_target->getApplicationSampleRate() != 0)) {
        m_sampleRate = m_target->getApplicationSampleRate();
    }

    PaStreamParameters ip, op;
    ip.device = Pa_GetDefaultInputDevice();
    op.device = Pa_GetDefaultOutputDevice();
    ip.channelCount = 2;
    op.channelCount = 2;
    ip.sampleFormat = paFloat32;
    op.sampleFormat = paFloat32;
//    ip.suggestedLatency = 1.0; //!!! was 0.2
//    op.suggestedLatency = 1.0; //!!! was 0.2
    ip.suggestedLatency = 0.2;
    op.suggestedLatency = 0.2;
    ip.hostApiSpecificStreamInfo = 0;
    op.hostApiSpecificStreamInfo = 0;
    err = Pa_OpenStream(&m_stream, &ip, &op, m_sampleRate,
                        paFramesPerBufferUnspecified,
                        paNoFlag, processStatic, this);

    if (err != paNoError) {
        err = Pa_OpenStream(&m_stream, &ip, &op, m_sampleRate, 1024,
                            paNoFlag, processStatic, this);
	m_bufferSize = 1024;
    }

    if (err != paNoError) {
	cerr << "ERROR: PortAudioIO: Failed to open PortAudio stream: " << Pa_GetErrorText(err) << endl;
	m_stream = 0;
	Pa_Terminate();
	return;
    }

    const PaStreamInfo *info = Pa_GetStreamInfo(m_stream);
    m_outputLatency = int(info->outputLatency * m_sampleRate + 0.001);
    m_inputLatency = int(info->inputLatency * m_sampleRate + 0.001);
    if (m_bufferSize == 0) m_bufferSize = m_outputLatency;

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
	m_source->setSystemPlaybackSampleRate(m_sampleRate);
	m_source->setSystemPlaybackLatency(m_outputLatency);
    }

    if (m_target) {
	m_target->setSystemRecordBlockSize(m_bufferSize);
	m_target->setSystemRecordSampleRate(m_sampleRate);
	m_target->setSystemRecordLatency(m_inputLatency);
    }
}

PortAudioIO::~PortAudioIO()
{
    if (m_stream) {
	PaError err;
	err = Pa_CloseStream(m_stream);
	if (err != paNoError) {
	    cerr << "ERROR: PortAudioIO: Failed to close PortAudio stream" << endl;
	}
	Pa_Terminate();
    }
}

bool
PortAudioIO::isSourceOK() const
{
    return (m_stream != 0);
}

bool
PortAudioIO::isTargetOK() const
{
    return (m_stream != 0);
}

double
PortAudioIO::getCurrentTime() const
{
    if (!m_stream) return 0.0;
    else return Pa_GetStreamTime(m_stream);
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
    
    float *input = (float *)inputBuffer;
    float *output = (float *)outputBuffer;

    assert(nframes <= m_bufferSize);

    static float **tmpbuf = 0;
    static int tmpbufch = 0;
    static int tmpbufsz = 0;

    int sourceChannels = m_source ? m_source->getApplicationChannelCount() : 0;
    if (sourceChannels == 0) {
        v_zero(output, nframes * 2);
        return 0;
    }

    int targetChannels = m_target ? m_target->getApplicationChannelCount() : 0;

    if (!tmpbuf || tmpbufch != sourceChannels || tmpbufsz < m_bufferSize) {

	if (tmpbuf) {
	    for (int i = 0; i < tmpbufch; ++i) {
		delete[] tmpbuf[i];
	    }
	    delete[] tmpbuf;
	}

	tmpbufch = sourceChannels;
	tmpbufsz = m_bufferSize;
	tmpbuf = new float *[tmpbufch];

	for (int i = 0; i < tmpbufch; ++i) {
	    tmpbuf[i] = new float[tmpbufsz];
	}
    }
	
    //!!! susceptible to vectorisation

    float peakLeft, peakRight;

    if (m_source) {

        int received = m_source->getSourceSamples(nframes, tmpbuf);
        for (int c = 0; c < sourceChannels; ++c) {
            for (int i = received; i < nframes; ++i) {
                tmpbuf[c][i] = 0.f;
            }
        }
        
        peakLeft = 0.0, peakRight = 0.0;

        for (int ch = 0; ch < 2; ++ch) {
	
            float peak = 0.0;

            if (ch < sourceChannels) {

                // PortAudio samples are interleaved
                for (int i = 0; i < nframes; ++i) {
                    output[i * 2 + ch] = tmpbuf[ch][i] * m_outputGain;
                    float sample = fabsf(output[i * 2 + ch]);
                    if (sample > peak) peak = sample;
                }

            } else if (ch == 1 && sourceChannels == 1) {

                for (int i = 0; i < nframes; ++i) {
                    output[i * 2 + ch] = tmpbuf[0][i] * m_outputGain;
                    float sample = fabsf(output[i * 2 + ch]);
                    if (sample > peak) peak = sample;
                }

            } else { 
                for (int i = 0; i < nframes; ++i) {
                    output[i * 2 + ch] = 0;
                }
           }

            if (ch == 0) peakLeft = peak;
            if (ch > 0 || sourceChannels == 1) peakRight = peak;
        }

        m_source->setOutputLevels(peakLeft, peakRight);

    } else {
        for (int ch = 0; ch < 2; ++ch) {
            for (int i = 0; i < nframes; ++i) {
                output[i * 2 + ch] = 0;
            }
        }
    }

    if (m_target) {

        peakLeft = 0.0, peakRight = 0.0;

        for (int ch = 0; ch < 2; ++ch) {
	
            float peak = 0.0;

            if (ch < targetChannels) {

                // PortAudio samples are interleaved
                for (int i = 0; i < nframes; ++i) {
                    tmpbuf[ch][i] = input[i * 2 + ch];
                    float sample = fabsf(input[i * 2 + ch]);
                    if (sample > peak) peak = sample;
                }

            } else if (ch == 1 && targetChannels == 1) {

                for (int i = 0; i < nframes; ++i) {
                    tmpbuf[0][i] = input[i * 2 + ch];
                    float sample = fabsf(input[i * 2 + ch]);
                    if (sample > peak) peak = sample;
                }

            } else {
                for (int i = 0; i < nframes; ++i) {
                    tmpbuf[ch][i] = 0;
                }
            }

            if (ch == 0) peakLeft = peak;
            if (ch > 0 || targetChannels == 1) peakRight = peak;
        }

        m_target->putSamples(nframes, tmpbuf);
        m_target->setInputLevels(peakLeft, peakRight);
    }

    return 0;
}

}

#endif /* HAVE_PORTAUDIO */

