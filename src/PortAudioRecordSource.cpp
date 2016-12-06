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

#include "PortAudioRecordSource.h"
#include "ApplicationRecordTarget.h"

#include <iostream>
#include <cassert>
#include <cmath>
#include <climits>

//#define DEBUG_AUDIO_PORT_AUDIO_SOURCE 1

using namespace std;

namespace breakfastquay {

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

PortAudioRecordSource::PortAudioRecordSource(ApplicationRecordTarget *target) :
    SystemRecordSource(target),
    m_stream(0),
    m_bufferSize(0),
    m_sampleRate(0),
    m_latency(0),
    m_suspended(false)
{
    cerr << "PortAudioRecordSource::PortAudioRecordSource" << endl;

    PaError err;

    err = Pa_Initialize();
    if (err != paNoError) {
	cerr << "ERROR: PortAudioRecordSource: Failed to initialize PortAudio" << endl;
	return;
    }

    m_bufferSize = 1024;

    PaStreamParameters p;
    p.device = Pa_GetDefaultInputDevice();

    const PaDeviceInfo *inInfo = Pa_GetDeviceInfo(p.device);
    m_sampleRate = int(round(inInfo->defaultSampleRate));

    if (m_target && (m_target->getApplicationSampleRate() != 0)) {
	m_sampleRate = m_target->getApplicationSampleRate();
    } else if (m_sampleRate == 0) {
        m_sampleRate = 44100;
    }
    
    p.channelCount = 2;
    p.sampleFormat = paFloat32;
    p.suggestedLatency = 0.2;
    p.hostApiSpecificStreamInfo = 0;
    err = Pa_OpenStream(&m_stream, &p, 0, m_sampleRate,
                        paFramesPerBufferUnspecified,
                        paNoFlag, processStatic, this);

    if (err != paNoError) {
	cerr << "ERROR: PortAudioRecordSource: Failed to open PortAudio stream" << endl;
	m_stream = 0;
	Pa_Terminate();
	return;
    }

    const PaStreamInfo *info = Pa_GetStreamInfo(m_stream);
    m_latency = int(info->inputLatency * m_sampleRate + 0.001);
    if (m_bufferSize < m_latency) m_bufferSize = m_latency;

    enableRT(m_stream);

    err = Pa_StartStream(m_stream);

    if (err != paNoError) {
	cerr << "ERROR: PortAudioRecordSource: Failed to start PortAudio stream" << endl;
	Pa_CloseStream(m_stream);
	m_stream = 0;
	Pa_Terminate();
	return;
    }

    if (m_target) {
	cerr << "PortAudioRecordSource: block size " << m_bufferSize << endl;
	m_target->setSystemRecordBlockSize(m_bufferSize);
	m_target->setSystemRecordSampleRate(m_sampleRate);
	m_target->setSystemRecordLatency(m_latency);
    }
}

PortAudioRecordSource::~PortAudioRecordSource()
{
    if (m_stream) {
	PaError err;
	err = Pa_CloseStream(m_stream);
	if (err != paNoError) {
	    cerr << "ERROR: PortAudioRecordSource: Failed to close PortAudio stream" << endl;
	}
	Pa_Terminate();
    }
}

bool
PortAudioRecordSource::isSourceOK() const
{
    return (m_stream != 0);
}

void
PortAudioRecordSource::suspend()
{
    if (m_suspended || !m_stream) return;
    PaError err = Pa_AbortStream(m_stream);
    if (err != paNoError) {
        cerr << "ERROR: PortAudioIO: Failed to abort PortAudio stream" << endl;
    }
    m_suspended = true;
#ifdef DEBUG_AUDIO_PORT_AUDIO_SOURCE
    cerr << "suspended" << endl;
#endif
}

void
PortAudioRecordSource::resume()
{
    if (!m_suspended || !m_stream) return;
    PaError err = Pa_StartStream(m_stream);
    if (err != paNoError) {
        cerr << "ERROR: PortAudioIO: Failed to restart PortAudio stream" << endl;
    }
    m_suspended = false;
#ifdef DEBUG_AUDIO_PORT_AUDIO_SOURCE
    cerr << "resumed" << endl;
#endif
}

int
PortAudioRecordSource::processStatic(const void *input, void *output,
                                    unsigned long nframes,
                                    const PaStreamCallbackTimeInfo *timeInfo,
                                    PaStreamCallbackFlags flags, void *data)
{
    return ((PortAudioRecordSource *)data)->process(input, output,
                                                   nframes, timeInfo,
                                                   flags);
}

int
PortAudioRecordSource::process(const void *inputBuffer, void *,
                               unsigned long pa_nframes,
                               const PaStreamCallbackTimeInfo *,
                               PaStreamCallbackFlags)
{
#ifdef DEBUG_AUDIO_PORT_AUDIO_SOURCE    
    cout << "PortAudioRecordSource::process(" << pa_nframes << ")" << endl;
#endif

    if (!m_target) return 0;

    if (pa_nframes > INT_MAX) pa_nframes = 0;
    int nframes = int(pa_nframes);
    
    float *input = (float *)inputBuffer;

    assert(nframes <= m_bufferSize);

    static float **tmpbuf = 0;
    static int tmpbufch = 0;
    static int tmpbufsz = 0;

    int channels = m_target->getApplicationChannelCount();

    if (!tmpbuf || tmpbufch != channels || tmpbufsz < m_bufferSize) {

	if (tmpbuf) {
	    for (int i = 0; i < tmpbufch; ++i) {
		delete[] tmpbuf[i];
	    }
	    delete[] tmpbuf;
	}

	tmpbufch = channels;
	tmpbufsz = m_bufferSize;
	tmpbuf = new float *[tmpbufch];

	for (int i = 0; i < tmpbufch; ++i) {
	    tmpbuf[i] = new float[tmpbufsz];
	}
    }
	
    float peakLeft = 0.0, peakRight = 0.0;

    //!!! susceptible to vectorisation

    for (int ch = 0; ch < 2; ++ch) {
	
	float peak = 0.0;

	if (ch < channels) {

	    // PortAudio samples are interleaved
	    for (int i = 0; i < nframes; ++i) {
		tmpbuf[ch][i] = input[i * 2 + ch];
		float sample = fabsf(input[i * 2 + ch]);
		if (sample > peak) peak = sample;
	    }

	} else if (ch == 1 && channels == 1) {

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
	if (ch > 0 || channels == 1) peakRight = peak;
    }

    m_target->putSamples(nframes, tmpbuf);
    m_target->setInputLevels(peakLeft, peakRight);

    return 0;
}

}
#endif /* HAVE_PORTAUDIO */

