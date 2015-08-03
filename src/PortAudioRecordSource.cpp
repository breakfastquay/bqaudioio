/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifdef HAVE_PORTAUDIO

#include "PortAudioRecordSource.h"
#include "ApplicationRecordTarget.h"

#include <iostream>
#include <cassert>
#include <cmath>

#define DEBUG_AUDIO_PORT_AUDIO_SOURCE 1

namespace breakfastquay {

#ifdef __LINUX__
extern "C" {
void
PaAlsa_EnableRealtimeScheduling(PaStream *, int);
}
#endif

static void
enableRT(PaStream *stream) {
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
    m_latency(0)
{
    std::cerr << "PortAudioRecordSource::PortAudioRecordSource" << std::endl;

    PaError err;

    err = Pa_Initialize();
    if (err != paNoError) {
	std::cerr << "ERROR: PortAudioRecordSource: Failed to initialize PortAudio" << std::endl;
	return;
    }

    m_bufferSize = 1024;
    m_sampleRate = 44100;
    if (m_target && (m_target->getApplicationSampleRate() != 0)) {
	m_sampleRate = m_target->getApplicationSampleRate();
    }

    PaStreamParameters p;
    p.device = Pa_GetDefaultInputDevice();
    p.channelCount = 2;
    p.sampleFormat = paFloat32;
    p.suggestedLatency = 1.0; //!!! was 0.2
    p.hostApiSpecificStreamInfo = 0;
    err = Pa_OpenStream(&m_stream, &p, 0, m_sampleRate,
                        paFramesPerBufferUnspecified,
                        paNoFlag, processStatic, this);

    if (err != paNoError) {
	std::cerr << "ERROR: PortAudioRecordSource: Failed to open PortAudio stream" << std::endl;
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
	std::cerr << "ERROR: PortAudioRecordSource: Failed to start PortAudio stream" << std::endl;
	Pa_CloseStream(m_stream);
	m_stream = 0;
	Pa_Terminate();
	return;
    }

    if (m_target) {
	std::cerr << "PortAudioRecordSource: block size " << m_bufferSize << std::endl;
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
	    std::cerr << "ERROR: PortAudioRecordSource: Failed to close PortAudio stream" << std::endl;
	}
	Pa_Terminate();
    }
}

bool
PortAudioRecordSource::isSourceOK() const
{
    return (m_stream != 0);
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
PortAudioRecordSource::process(const void *inputBuffer, void *outputBuffer,
                              unsigned long nframes,
                              const PaStreamCallbackTimeInfo *,
                              PaStreamCallbackFlags)
{
#ifdef DEBUG_AUDIO_PORT_AUDIO_SOURCE    
    std::cout << "PortAudioRecordSource::process(" << nframes << ")" << std::endl;
#endif

    if (!m_target) return 0;

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

