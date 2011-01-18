/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifdef HAVE_PORTAUDIO

#include "AudioPortAudioTarget.h"
#include "AudioCallbackPlaySource.h"

#include "dsp/Resampler.h"
#include "system/Allocators.h"
#include "system/VectorOps.h"

#include <iostream>
#include <cassert>
#include <cmath>

namespace Turbot {

//#define DEBUG_AUDIO_PORT_AUDIO_TARGET 1

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

AudioPortAudioTarget::AudioPortAudioTarget(AudioCallbackPlaySource *source) :
    AudioCallbackPlayTarget(source),
    m_stream(0),
    m_resampler(0),
    m_bufferSize(0),
    m_sampleRate(0),
    m_latency(0)
{
    PaError err;

    err = Pa_Initialize();
    if (err != paNoError) {
	std::cerr << "ERROR: AudioPortAudioTarget: Failed to initialize PortAudio" << std::endl;
	return;
    }

    m_bufferSize = 0;
    m_sampleRate = 44100;
    if (m_source && (m_source->getSourceSampleRate() != 0)) {
	m_sampleRate = m_source->getSourceSampleRate();
    }

    std::cerr << "Sample rate: " << m_sampleRate << std::endl;

    PaStreamParameters op;
    op.device = Pa_GetDefaultOutputDevice();

    std::cerr << "device: " << op.device << std::endl;
    
    op.channelCount = 2;
    op.sampleFormat = paFloat32;
    op.suggestedLatency = 1.0; //!!! was 0.2
//    op.suggestedLatency = 0.2;
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
	std::cerr << "ERROR: AudioPortAudioTarget: Failed to open PortAudio stream: " << Pa_GetErrorText(err) << std::endl;
	m_stream = 0;
	Pa_Terminate();
	return;
    }

    const PaStreamInfo *info = Pa_GetStreamInfo(m_stream);
    m_latency = int(info->outputLatency * m_sampleRate + 0.001);
    if (m_bufferSize == 0) {
	m_bufferSize = m_latency;
	if (m_bufferSize == 0) {
//	    std::cerr << "AudioPortAudioTarget: stream is reporting zero latency, this can't be true" << std::endl;
	    m_bufferSize = 1024;
 	}
    }

    enableRT(m_stream);

    err = Pa_StartStream(m_stream);

    if (err != paNoError) {
	std::cerr << "ERROR: AudioPortAudioTarget: Failed to start PortAudio stream" << std::endl;
	Pa_CloseStream(m_stream);
	m_stream = 0;
	Pa_Terminate();
	return;
    }

    if (m_source) {
//	std::cerr << "AudioPortAudioTarget: block size " << m_bufferSize << std::endl;
	m_source->setTargetBlockSize(m_bufferSize);
	m_source->setTargetSampleRate(m_sampleRate);
	m_source->setTargetPlayLatency(m_latency);
    }
}

AudioPortAudioTarget::~AudioPortAudioTarget()
{
    if (m_stream) {
	PaError err;
	err = Pa_CloseStream(m_stream);
	if (err != paNoError) {
	    std::cerr << "ERROR: AudioPortAudioTarget: Failed to close PortAudio stream" << std::endl;
	}
	Pa_Terminate();
    }
}

bool
AudioPortAudioTarget::isTargetOK() const
{
    return (m_stream != 0);
}

double
AudioPortAudioTarget::getCurrentTime() const
{
    if (!m_stream) return 0.0;
    else return Pa_GetStreamTime(m_stream);
}

int
AudioPortAudioTarget::processStatic(const void *input, void *output,
                                    unsigned long nframes,
                                    const PaStreamCallbackTimeInfo *timeInfo,
                                    PaStreamCallbackFlags flags, void *data)
{
    return ((AudioPortAudioTarget *)data)->process(input, output,
                                                   nframes, timeInfo,
                                                   flags);
}

int
AudioPortAudioTarget::process(const void *, void *outputBuffer,
                              unsigned long nframes,
                              const PaStreamCallbackTimeInfo *,
                              PaStreamCallbackFlags)
{
#ifdef DEBUG_AUDIO_PORT_AUDIO_TARGET    
    std::cout << "AudioPortAudioTarget::process(" << nframes << ")" << std::endl;
#endif

    if (!m_source) return 0;

    float *output = (float *)outputBuffer;

    if (nframes > m_bufferSize) {
	std::cerr << "AudioPortAudioTarget::process: nframes = " << nframes
   	 	  << " against buffer size " << m_bufferSize << std::endl;
    }
    assert(nframes <= m_bufferSize);

    static float **tmpbuf = 0;
    static size_t tmpbufch = 0;
    static size_t tmpbufsz = 0;

    static float **resampbuf = 0;
    static size_t resampbufch = 0;
    static size_t resampbufsz = 0;

    std::cerr << "AudioPortAudioTarget" << std::endl;

    size_t sourceChannels = m_source->getSourceChannelCount();
    if (sourceChannels == 0) {
        std::cerr << "zero " << nframes*2 << std::endl;

        v_zero(output, nframes * 2);
        return 0;
    }

    if (!tmpbuf || tmpbufch != sourceChannels || tmpbufsz < m_bufferSize) {
        deallocate_channels(tmpbuf, tmpbufch);
	tmpbufch = sourceChannels;
	tmpbufsz = m_bufferSize;
        tmpbuf = allocate_channels<float>(tmpbufch, tmpbufsz);
    }

    if (m_source->getSourceSampleRate() == 0 ||
        m_source->getSourceSampleRate() == m_sampleRate) {
	
        m_source->getSourceSamples(nframes, tmpbuf);

    } else {

//        std::cerr << "resampling " << m_source->getSourceSampleRate() 
//                  << " -> " << m_sampleRate << std::endl;
        
        if (m_resampler && m_resampler->getChannelCount() != sourceChannels) {
            delete m_resampler;
            m_resampler = 0;
        }
        if (!m_resampler) {
            m_resampler = new Resampler
                (Resampler::FastestTolerable, sourceChannels);
        }

        float ratio = float(m_sampleRate) /
            float(m_source->getSourceSampleRate());

        if (!resampbuf || resampbufsz != int(tmpbufsz / ratio) + 1) {
            deallocate_channels(resampbuf, resampbufch);
            resampbufch = sourceChannels;
            resampbufsz = int(tmpbufsz / ratio) + 1;
            resampbuf = allocate_channels<float>(resampbufch, resampbufsz);
        }

        m_source->getSourceSamples(nframes / ratio, resampbuf);

        m_resampler->resample(resampbuf, tmpbuf, nframes / ratio, ratio);
    }

    float peakLeft = 0.0, peakRight = 0.0;

    for (size_t ch = 0; ch < 2; ++ch) {
	
	float peak = 0.0;

        std::cerr << "ch = " << ch << ", sourceChannels = " << sourceChannels << std::endl;

	if (ch < sourceChannels) {

	    // PortAudio samples are interleaved
	    for (size_t i = 0; i < nframes; ++i) {
		output[i * 2 + ch] = tmpbuf[ch][i] * m_outputGain;
		float sample = fabsf(output[i * 2 + ch]);
		if (sample > peak) peak = sample;
	    }

	} else if (ch == 1 && sourceChannels == 1) {

	    for (size_t i = 0; i < nframes; ++i) {
		output[i * 2 + ch] = tmpbuf[0][i] * m_outputGain;
		float sample = fabsf(output[i * 2 + ch]);
		if (sample > peak) peak = sample;
	    }

	} else {
	    for (size_t i = 0; i < nframes; ++i) {
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

