/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifdef HAVE_PORTAUDIO

#include "AudioPortAudioIO.h"
#include "AudioCallbackPlaySource.h"
#include "AudioCallbackRecordTarget.h"

#include <iostream>
#include <cassert>
#include <cmath>

namespace Turbot {

//#define DEBUG_AUDIO_PORT_AUDIO_IO 1

AudioPortAudioIO::AudioPortAudioIO(AudioCallbackRecordTarget *target,
				   AudioCallbackPlaySource *source) :
    AudioCallbackIO(target, source),
    m_stream(0),
    m_bufferSize(0),
    m_sampleRate(0),
    m_inputLatency(0),
    m_outputLatency(0)
{
    PaError err;

    err = Pa_Initialize();
    if (err != paNoError) {
	std::cerr << "ERROR: AudioPortAudioIO: Failed to initialize PortAudio" << std::endl;
	return;
    }

    m_bufferSize = 0;
    m_sampleRate = 44100;

    if (m_source && (m_source->getSourceSampleRate() != 0)) {
	m_sampleRate = m_source->getSourceSampleRate();
    } else if (m_target && (m_target->getPreferredSampleRate() != 0)) {
        m_sampleRate = m_target->getPreferredSampleRate();
    }

    PaStreamParameters ip, op;
    ip.device = Pa_GetDefaultInputDevice();
    op.device = Pa_GetDefaultOutputDevice();
    ip.channelCount = 2;
    op.channelCount = 2;
    ip.sampleFormat = paFloat32;
    op.sampleFormat = paFloat32;
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
	std::cerr << "ERROR: AudioPortAudioIO: Failed to open PortAudio stream" << std::endl;
	m_stream = 0;
	Pa_Terminate();
	return;
    }

    const PaStreamInfo *info = Pa_GetStreamInfo(m_stream);
    m_outputLatency = int(info->outputLatency * m_sampleRate + 0.001);
    m_inputLatency = int(info->inputLatency * m_sampleRate + 0.001);
    if (m_bufferSize == 0) m_bufferSize = m_outputLatency;

    err = Pa_StartStream(m_stream);

    if (err != paNoError) {
	std::cerr << "ERROR: AudioPortAudioIO: Failed to start PortAudio stream" << std::endl;
	Pa_CloseStream(m_stream);
	m_stream = 0;
	Pa_Terminate();
	return;
    }

    std::cerr << "AudioPortAudioIO: block size " << m_bufferSize << std::endl;

    if (m_source) {
	m_source->setTargetBlockSize(m_bufferSize);
	m_source->setTargetSampleRate(m_sampleRate);
	m_source->setTargetPlayLatency(m_outputLatency);
    }

    if (m_target) {
	m_target->setSourceBlockSize(m_bufferSize);
	m_target->setSourceSampleRate(m_sampleRate);
	m_target->setSourceRecordLatency(m_inputLatency);
    }
}

AudioPortAudioIO::~AudioPortAudioIO()
{
    if (m_stream) {
	PaError err;
	err = Pa_CloseStream(m_stream);
	if (err != paNoError) {
	    std::cerr << "ERROR: AudioPortAudioIO: Failed to close PortAudio stream" << std::endl;
	}
	Pa_Terminate();
    }
}

bool
AudioPortAudioIO::isSourceOK() const
{
    return (m_stream != 0);
}

bool
AudioPortAudioIO::isTargetOK() const
{
    return (m_stream != 0);
}

double
AudioPortAudioIO::getCurrentTime() const
{
    if (!m_stream) return 0.0;
    else return Pa_GetStreamTime(m_stream);
}

int
AudioPortAudioIO::processStatic(const void *input, void *output,
                                unsigned long nframes,
                                const PaStreamCallbackTimeInfo *timeInfo,
                                PaStreamCallbackFlags flags, void *data)
{
    return ((AudioPortAudioIO *)data)->process(input, output,
                                               nframes, timeInfo,
                                               flags);
}

int
AudioPortAudioIO::process(const void *inputBuffer, void *outputBuffer,
                          unsigned long nframes,
                          const PaStreamCallbackTimeInfo *,
                          PaStreamCallbackFlags)
{
#ifdef DEBUG_AUDIO_PORT_AUDIO_IO    
    std::cout << "AudioPortAudioIO::process(" << nframes << ")" << std::endl;
#endif

    if (!m_source && !m_target) return 0;

    float *input = (float *)inputBuffer;
    float *output = (float *)outputBuffer;

    assert(nframes <= m_bufferSize);

    static float **tmpbuf = 0;
    static size_t tmpbufch = 0;
    static size_t tmpbufsz = 0;

    size_t sourceChannels = m_source ? m_source->getSourceChannelCount() : 0;
    size_t targetChannels = m_target ? m_target->getChannelCount() : 0;

    // Because we offer pan, we always want at least 2 channels
    if (sourceChannels < 2) sourceChannels = 2;

    if (!tmpbuf || tmpbufch != sourceChannels || tmpbufsz < m_bufferSize) {

	if (tmpbuf) {
	    for (size_t i = 0; i < tmpbufch; ++i) {
		delete[] tmpbuf[i];
	    }
	    delete[] tmpbuf;
	}

	tmpbufch = sourceChannels;
	tmpbufsz = m_bufferSize;
	tmpbuf = new float *[tmpbufch];

	for (size_t i = 0; i < tmpbufch; ++i) {
	    tmpbuf[i] = new float[tmpbufsz];
	}
    }
	
    //!!! susceptible to vectorisation

    float peakLeft, peakRight;

    if (m_source) {

        m_source->getSourceSamples(nframes, tmpbuf);
    
        peakLeft = 0.0, peakRight = 0.0;

        for (size_t ch = 0; ch < 2; ++ch) {
	
            float peak = 0.0;

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

    } else {
        for (size_t ch = 0; ch < 2; ++ch) {
            for (size_t i = 0; i < nframes; ++i) {
                output[i * 2 + ch] = 0;
            }
        }
    }

    if (m_target) {

        peakLeft = 0.0, peakRight = 0.0;

        for (size_t ch = 0; ch < 2; ++ch) {
	
            float peak = 0.0;

            if (ch < targetChannels) {

                // PortAudio samples are interleaved
                for (size_t i = 0; i < nframes; ++i) {
                    tmpbuf[ch][i] = input[i * 2 + ch];
                    float sample = fabsf(input[i * 2 + ch]);
                    if (sample > peak) peak = sample;
                }

            } else if (ch == 1 && targetChannels == 1) {

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
            if (ch > 0 || targetChannels == 1) peakRight = peak;
        }

        m_target->putSamples(nframes, tmpbuf);
        m_target->setInputLevels(peakLeft, peakRight);
    }

    return 0;
}

}

#endif /* HAVE_PORTAUDIO */

