/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifdef HAVE_PORTAUDIO

#include "AudioPortAudioSource.h"
#include "AudioCallbackRecordTarget.h"

#include <iostream>
#include <cassert>
#include <cmath>

#define DEBUG_AUDIO_PORT_AUDIO_SOURCE 1

namespace Turbot {

AudioPortAudioSource::AudioPortAudioSource(AudioCallbackRecordTarget *target) :
    AudioCallbackRecordSource(target),
    m_stream(0),
    m_bufferSize(0),
    m_sampleRate(0),
    m_latency(0)
{
    std::cerr << "AudioPortAudioSource::AudioPortAudioSource" << std::endl;

    PaError err;

    err = Pa_Initialize();
    if (err != paNoError) {
	std::cerr << "ERROR: AudioPortAudioSource: Failed to initialize PortAudio" << std::endl;
	return;
    }

    m_bufferSize = 1024;
    m_sampleRate = 44100;
    if (m_target && (m_target->getPreferredSampleRate() != 0)) {
	m_sampleRate = m_target->getPreferredSampleRate();
    }

    PaStreamParameters p;
    p.device = Pa_GetDefaultInputDevice();
    p.channelCount = 2;
    p.sampleFormat = paFloat32;
    p.suggestedLatency = 0.2;
    p.hostApiSpecificStreamInfo = 0;
    err = Pa_OpenStream(&m_stream, &p, 0, m_sampleRate,
                        paFramesPerBufferUnspecified,
                        paNoFlag, processStatic, this);

    if (err != paNoError) {
	std::cerr << "ERROR: AudioPortAudioSource: Failed to open PortAudio stream" << std::endl;
	m_stream = 0;
	Pa_Terminate();
	return;
    }

    const PaStreamInfo *info = Pa_GetStreamInfo(m_stream);
    m_latency = int(info->inputLatency * m_sampleRate + 0.001);
    if (m_bufferSize < m_latency) m_bufferSize = m_latency;

    err = Pa_StartStream(m_stream);

    if (err != paNoError) {
	std::cerr << "ERROR: AudioPortAudioSource: Failed to start PortAudio stream" << std::endl;
	Pa_CloseStream(m_stream);
	m_stream = 0;
	Pa_Terminate();
	return;
    }

    if (m_target) {
	std::cerr << "AudioPortAudioSource: block size " << m_bufferSize << std::endl;
	m_target->setSourceBlockSize(m_bufferSize);
	m_target->setSourceSampleRate(m_sampleRate);
	m_target->setSourceRecordLatency(m_latency);
    }
}

AudioPortAudioSource::~AudioPortAudioSource()
{
    if (m_stream) {
	PaError err;
	err = Pa_CloseStream(m_stream);
	if (err != paNoError) {
	    std::cerr << "ERROR: AudioPortAudioSource: Failed to close PortAudio stream" << std::endl;
	}
	Pa_Terminate();
    }
}

bool
AudioPortAudioSource::isSourceOK() const
{
    return (m_stream != 0);
}

int
AudioPortAudioSource::processStatic(const void *input, void *output,
                                    unsigned long nframes,
                                    const PaStreamCallbackTimeInfo *timeInfo,
                                    PaStreamCallbackFlags flags, void *data)
{
    return ((AudioPortAudioSource *)data)->process(input, output,
                                                   nframes, timeInfo,
                                                   flags);
}

int
AudioPortAudioSource::process(const void *inputBuffer, void *outputBuffer,
                              unsigned long nframes,
                              const PaStreamCallbackTimeInfo *,
                              PaStreamCallbackFlags)
{
#ifdef DEBUG_AUDIO_PORT_AUDIO_SOURCE    
    std::cout << "AudioPortAudioSource::process(" << nframes << ")" << std::endl;
#endif

    if (!m_target) return 0;

    float *input = (float *)inputBuffer;

    assert(nframes <= m_bufferSize);

    static float **tmpbuf = 0;
    static size_t tmpbufch = 0;
    static size_t tmpbufsz = 0;

    size_t channels = m_target->getChannelCount();

    if (!tmpbuf || tmpbufch != channels || tmpbufsz < m_bufferSize) {

	if (tmpbuf) {
	    for (size_t i = 0; i < tmpbufch; ++i) {
		delete[] tmpbuf[i];
	    }
	    delete[] tmpbuf;
	}

	tmpbufch = channels;
	tmpbufsz = m_bufferSize;
	tmpbuf = new float *[tmpbufch];

	for (size_t i = 0; i < tmpbufch; ++i) {
	    tmpbuf[i] = new float[tmpbufsz];
	}
    }
	
    float peakLeft = 0.0, peakRight = 0.0;

    //!!! susceptible to vectorisation

    for (size_t ch = 0; ch < 2; ++ch) {
	
	float peak = 0.0;

	if (ch < channels) {

	    // PortAudio samples are interleaved
	    for (size_t i = 0; i < nframes; ++i) {
		tmpbuf[ch][i] = input[i * 2 + ch];
		float sample = fabsf(input[i * 2 + ch]);
		if (sample > peak) peak = sample;
	    }

	} else if (ch == 1 && channels == 1) {

	    for (size_t i = 0; i < nframes; ++i) {
		tmpbuf[0][i] = input[i * 2 + ch];
		float sample = fabsf(input[i * 2 + ch]);
		if (sample > peak) peak = sample;
	    }

	} else {
	    for (size_t i = 0; i < nframes; ++i) {
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

