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

#include "PortAudioPlaybackTarget.h"
#include "ApplicationPlaybackSource.h"

#include "bqresample/Resampler.h"
#include "bqvec/Allocators.h"
#include "bqvec/VectorOps.h"

#include <iostream>
#include <cassert>
#include <cmath>
#include <climits>

using namespace std;

namespace breakfastquay {

//#define DEBUG_AUDIO_PORT_AUDIO_TARGET 1

#ifdef __LINUX__
extern "C" {
void
PaAlsa_EnableRealtimeScheduling(PaStream *, int);
}
#endif

static void
enableRT(PaStream *
#ifdef __LINUX__
         stream
#endif
    ) {
#ifdef __LINUX__
    // This will link only if the PA ALSA host API is linked statically
    PaAlsa_EnableRealtimeScheduling(stream, 1);
#endif
}

PortAudioPlaybackTarget::PortAudioPlaybackTarget(ApplicationPlaybackSource *source) :
    SystemPlaybackTarget(source),
    m_stream(0),
    m_resampler(0),
    m_bufferSize(0),
    m_sampleRate(0),
    m_latency(0),
    m_suspended(false)
{
    PaError err;

    err = Pa_Initialize();
    if (err != paNoError) {
	cerr << "ERROR: PortAudioPlaybackTarget: Failed to initialize PortAudio" << endl;
	return;
    }

    m_bufferSize = 0;

    PaStreamParameters op;
    op.device = Pa_GetDefaultOutputDevice();

    const PaDeviceInfo *outInfo = Pa_GetDeviceInfo(op.device);
    m_sampleRate = int(round(outInfo->defaultSampleRate));

    if (m_source && (m_source->getApplicationSampleRate() != 0)) {
	m_sampleRate = m_source->getApplicationSampleRate();
    } else if (m_sampleRate == 0) {
        m_sampleRate = 44100;
    }
    
    op.channelCount = 2;
    op.sampleFormat = paFloat32;
    op.suggestedLatency = 0.2;
    op.hostApiSpecificStreamInfo = 0;
    err = Pa_OpenStream(&m_stream, 0, &op, m_sampleRate,
                        paFramesPerBufferUnspecified,
                        paNoFlag, processStatic, this);

    if (err != paNoError) {
        err = Pa_OpenStream(&m_stream, 0, &op, m_sampleRate, 1024,
                            paNoFlag, processStatic, this);
	m_bufferSize = 1024;
    }

    if (err != paNoError) {
	cerr << "ERROR: PortAudioPlaybackTarget: Failed to open PortAudio stream: " << Pa_GetErrorText(err) << endl;
	m_stream = 0;
	Pa_Terminate();
	return;
    }

    const PaStreamInfo *info = Pa_GetStreamInfo(m_stream);
    m_latency = int(info->outputLatency * m_sampleRate + 0.001);
    if (m_bufferSize == 0) {
	m_bufferSize = m_latency;
	if (m_bufferSize == 0) {
//	    cerr << "PortAudioPlaybackTarget: stream is reporting zero latency, this can't be true" << endl;
	    m_bufferSize = 1024;
 	}
    }

    enableRT(m_stream);

    err = Pa_StartStream(m_stream);

    if (err != paNoError) {
	cerr << "ERROR: PortAudioPlaybackTarget: Failed to start PortAudio stream" << endl;
	Pa_CloseStream(m_stream);
	m_stream = 0;
	Pa_Terminate();
	return;
    }

    if (m_source) {
//	cerr << "PortAudioPlaybackTarget: block size " << m_bufferSize << endl;
	m_source->setSystemPlaybackBlockSize(m_bufferSize);
	m_source->setSystemPlaybackSampleRate(m_sampleRate);
	m_source->setSystemPlaybackLatency(m_latency);
    }
}

PortAudioPlaybackTarget::~PortAudioPlaybackTarget()
{
    if (m_stream) {
	PaError err;
	err = Pa_CloseStream(m_stream);
	if (err != paNoError) {
	    cerr << "ERROR: PortAudioPlaybackTarget: Failed to close PortAudio stream" << endl;
	}
	Pa_Terminate();
    }
}

bool
PortAudioPlaybackTarget::isTargetOK() const
{
    return (m_stream != 0);
}

double
PortAudioPlaybackTarget::getCurrentTime() const
{
    if (!m_stream) return 0.0;
    else return Pa_GetStreamTime(m_stream);
}

void
PortAudioPlaybackTarget::suspend()
{
    if (m_suspended || !m_stream) return;
    PaError err = Pa_AbortStream(m_stream);
    if (err != paNoError) {
        cerr << "ERROR: PortAudioIO: Failed to abort PortAudio stream" << endl;
    }
    m_suspended = true;
#ifdef DEBUG_AUDIO_PORT_AUDIO_TARGET
    cerr << "suspended" << endl;
#endif
}

void
PortAudioPlaybackTarget::resume()
{
    if (!m_suspended || !m_stream) return;
    PaError err = Pa_StartStream(m_stream);
    if (err != paNoError) {
        cerr << "ERROR: PortAudioIO: Failed to restart PortAudio stream" << endl;
    }
    m_suspended = false;
#ifdef DEBUG_AUDIO_PORT_AUDIO_TARGET
    cerr << "resumed" << endl;
#endif
}

int
PortAudioPlaybackTarget::processStatic(const void *input, void *output,
                                    unsigned long nframes,
                                    const PaStreamCallbackTimeInfo *timeInfo,
                                    PaStreamCallbackFlags flags, void *data)
{
    return ((PortAudioPlaybackTarget *)data)->process(input, output,
                                                      nframes, timeInfo,
                                                      flags);
}

int
PortAudioPlaybackTarget::process(const void *, void *outputBuffer,
                                 unsigned long pa_nframes,
                                 const PaStreamCallbackTimeInfo *,
                                 PaStreamCallbackFlags)
{
#ifdef DEBUG_AUDIO_PORT_AUDIO_TARGET    
    cout << "PortAudioPlaybackTarget::process(" << pa_nframes << ")" << endl;
#endif

    if (!m_source) return 0;

    if (pa_nframes > INT_MAX) pa_nframes = 0;
    int nframes = int(pa_nframes);

    float *output = (float *)outputBuffer;

    if (nframes > m_bufferSize) {
	cerr << "PortAudioPlaybackTarget::process: nframes = " << nframes
   	 	  << " against buffer size " << m_bufferSize << endl;
    }
    assert(nframes <= m_bufferSize);

    static float **tmpbuf = 0;
    static int tmpbufch = 0;
    static int tmpbufsz = 0;

    static float **resampbuf = 0;
    static int resampbufch = 0;
    static int resampbufsz = 0;

    int sourceChannels = m_source->getApplicationChannelCount();
    if (sourceChannels == 0) {
        v_zero(output, nframes * 2);
        return 0;
    }

    if (!tmpbuf || tmpbufch != sourceChannels || tmpbufsz < m_bufferSize) {
        deallocate_channels(tmpbuf, tmpbufch);
	tmpbufch = sourceChannels;
	tmpbufsz = m_bufferSize;
        tmpbuf = allocate_channels<float>(tmpbufch, tmpbufsz);
    }

    if (m_source->getApplicationSampleRate() == 0 ||
        m_source->getApplicationSampleRate() == m_sampleRate) {
	
        int received = m_source->getSourceSamples(nframes, tmpbuf);
        for (int c = 0; c < sourceChannels; ++c) {
            for (int i = received; i < nframes; ++i) {
                tmpbuf[c][i] = 0.f;
            }
        }

    } else {

//        cerr << "resampling " << m_source->getApplicationSampleRate() 
//                  << " -> " << m_sampleRate << endl;
        
        if (m_resampler && m_resampler->getChannelCount() != sourceChannels) {
            delete m_resampler;
            m_resampler = 0;
        }
        if (!m_resampler) {
            m_resampler = new Resampler
                (Resampler::FastestTolerable, sourceChannels);
        }

        double ratio = double(m_sampleRate) /
            double(m_source->getApplicationSampleRate());

        if (!resampbuf || resampbufsz != int(tmpbufsz / ratio) + 1) {
            deallocate_channels(resampbuf, resampbufch);
            resampbufch = sourceChannels;
            resampbufsz = int(tmpbufsz / ratio) + 1;
            resampbuf = allocate_channels<float>(resampbufch, resampbufsz);
        }

        int received = m_source->getSourceSamples(int(nframes / ratio), resampbuf);
        for (int c = 0; c < sourceChannels; ++c) {
            for (int i = received; i < int(nframes / ratio); ++i) {
                resampbuf[c][i] = 0.f;
            }
        }

        m_resampler->resample(resampbuf, tmpbuf, int(nframes / ratio), ratio);
    }

    float peakLeft = 0.0, peakRight = 0.0;

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

    return 0;
}

}

#endif /* HAVE_PORTAUDIO */

