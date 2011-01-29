/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifdef HAVE_JACK

#include "JACKAudioIO.h"
#include "DynamicJACK.h"
#include "ApplicationPlaybackSource.h"
#include "ApplicationRecordTarget.h"

#include <iostream>
#include <cmath>
#include <cstdio>

//#define DEBUG_AUDIO_JACK_IO 1

namespace Turbot {

JACKAudioIO::JACKAudioIO(ApplicationRecordTarget *target,
			 ApplicationPlaybackSource *source) :
    SystemAudioIO(target, source),
    m_client(0),
    m_bufferSize(0),
    m_sampleRate(0)
{
    char name[20];
    strcpy(name, "turbot");
    m_client = jack_client_new(name);

    if (!m_client) {
	sprintf(name, "turbot (%d)", (int)getpid());
	m_client = jack_client_new(name);
	if (!m_client) {
	    std::cerr
		<< "ERROR: JACKAudioIO: Failed to connect to JACK server"
		<< std::endl;
	}
    }

    if (!m_client) return;

    m_bufferSize = jack_get_buffer_size(m_client);
    m_sampleRate = jack_get_sample_rate(m_client);

    jack_set_xrun_callback(m_client, xrunStatic, this);
    jack_set_process_callback(m_client, processStatic, this);

    if (jack_activate(m_client)) {
	std::cerr << "ERROR: JACKAudioIO: Failed to activate JACK client"
		  << std::endl;
    }

    setup(2);
}

JACKAudioIO::~JACKAudioIO()
{
    if (m_client) {
	jack_deactivate(m_client);
	jack_client_close(m_client);
    }
}

bool
JACKAudioIO::isSourceOK() const
{
    return (m_client != 0);
}

bool
JACKAudioIO::isTargetOK() const
{
    return (m_client != 0);
}

double
JACKAudioIO::getCurrentTime() const
{
    if (m_client && m_sampleRate) {
        return double(jack_frame_time(m_client)) / double(m_sampleRate);
    } else {
        return 0.0;
    }
}

int
JACKAudioIO::processStatic(jack_nframes_t nframes, void *arg)
{
    return ((JACKAudioIO *)arg)->process(nframes);
}

int
JACKAudioIO::xrunStatic(void *arg)
{
    return ((JACKAudioIO *)arg)->xrun();
}

void
JACKAudioIO::setup(size_t channels)
{
    m_mutex.lock();

    if (m_source) {
        m_source->setTargetBlockSize(m_bufferSize);
        m_source->setTargetSampleRate(m_sampleRate);
    }
    if (m_target) {
        m_target->setSourceBlockSize(m_bufferSize);
        m_target->setSourceSampleRate(m_sampleRate);
        if (channels < m_target->getChannelCount()) {
            channels = m_target->getChannelCount();
        }
    }

    // Because we offer pan, we always want at least 2 channels
    if (channels < 2) channels = 2;

    if (channels == m_outputs.size() || !m_client) {
	m_mutex.unlock();
	return;
    }

    const char **playPorts =
	jack_get_ports(m_client, NULL, NULL,
		       JackPortIsPhysical | JackPortIsInput);
    const char **capPorts =
	jack_get_ports(m_client, NULL, NULL,
		       JackPortIsPhysical | JackPortIsOutput);

    size_t playPortCount = 0;
    while (playPorts && playPorts[playPortCount]) ++playPortCount;

    size_t capPortCount = 0;
    while (capPorts && capPorts[capPortCount]) ++capPortCount;

#ifdef DEBUG_AUDIO_JACK_IO    
    std::cerr << "JACKAudioIO::setup: have " << channels << " channels, " << capPortCount << " capture ports, " << playPortCount << " playback ports" << std::endl;
#endif

    if (m_source) {

        while (m_outputs.size() < channels) {
	
            char name[20];
            jack_port_t *port;

            sprintf(name, "out %ld", long(m_outputs.size() + 1));

            port = jack_port_register(m_client,
                                      name,
                                      JACK_DEFAULT_AUDIO_TYPE,
                                      JackPortIsOutput,
                                      0);

            if (!port) {
                std::cerr
                    << "ERROR: JACKAudioIO: Failed to create JACK output port "
                    << m_outputs.size() << std::endl;
                return;
            } else {
                m_source->setTargetPlayLatency(jack_port_get_latency(port));
            }

            if (m_outputs.size() < playPortCount) {
                jack_connect(m_client,
                             jack_port_name(port),
                             playPorts[m_outputs.size()]);
            }

            m_outputs.push_back(port);
        }
    }

    if (m_target) {

        while (m_inputs.size() < channels) {
	
            char name[20];
            jack_port_t *port;

            sprintf(name, "in %ld", long(m_inputs.size() + 1));

            port = jack_port_register(m_client,
                                      name,
                                      JACK_DEFAULT_AUDIO_TYPE,
                                      JackPortIsInput,
                                      0);

            if (!port) {
                std::cerr
                    << "ERROR: JACKAudioIO: Failed to create JACK input port "
                    << m_inputs.size() << std::endl;
                return;
            } else {
                m_target->setSourceRecordLatency(jack_port_get_latency(port));
            }

            if (m_inputs.size() < capPortCount) {
                jack_connect(m_client,
                             capPorts[m_inputs.size()],
                             jack_port_name(port));
            }

            m_inputs.push_back(port);
        }
    }

    while (m_outputs.size() > channels) {
	std::vector<jack_port_t *>::iterator itr = m_outputs.end();
	--itr;
	jack_port_t *port = *itr;
	if (port) jack_port_unregister(m_client, port);
	m_outputs.erase(itr);
    }

    while (m_inputs.size() > channels) {
	std::vector<jack_port_t *>::iterator itr = m_inputs.end();
	--itr;
	jack_port_t *port = *itr;
	if (port) jack_port_unregister(m_client, port);
	m_inputs.erase(itr);
    }

    m_mutex.unlock();
}

int
JACKAudioIO::process(jack_nframes_t nframes)
{
    if (!m_mutex.tryLock()) {
	return 0;
    }

    if (m_outputs.empty() && m_inputs.empty()) {
	m_mutex.unlock();
	return 0;
    }

#ifdef DEBUG_AUDIO_JACK_IO    
    std::cout << "JACKAudioIO::process(" << nframes << "): have a purpose in life" << std::endl;
#endif

#ifdef DEBUG_AUDIO_JACK_IO    
    if (m_bufferSize != nframes) {
	std::cerr << "WARNING: m_bufferSize != nframes (" << m_bufferSize << " != " << nframes << ")" << std::endl;
    }
#endif

    float **inbufs = (float **)alloca(m_inputs.size() * sizeof(float *));
    float **outbufs = (float **)alloca(m_outputs.size() * sizeof(float *));

    float peakLeft, peakRight;

    if (m_source) {

        for (size_t ch = 0; ch < m_outputs.size(); ++ch) {
            outbufs[ch] = (float *)jack_port_get_buffer(m_outputs[ch], nframes);
        }
        
	m_source->getSourceSamples(nframes, outbufs);

        peakLeft = 0.0; peakRight = 0.0;

        //!!! vectorisation

        for (size_t ch = 0; ch < m_outputs.size(); ++ch) {

            float peak = 0.0;

            for (size_t i = 0; i < nframes; ++i) {
                outbufs[ch][i] *= m_outputGain;
                float sample = fabsf(outbufs[ch][i]);
                if (sample > peak) peak = sample;
            }

            if (ch == 0) peakLeft = peak;
            if (ch > 0 || m_outputs.size() == 1) peakRight = peak;
        }
	    
        m_source->setOutputLevels(peakLeft, peakRight);

    } else {
	for (size_t ch = 0; ch < m_outputs.size(); ++ch) {
	    for (size_t i = 0; i < nframes; ++i) {
		outbufs[ch][i] = 0.0;
	    }
	}
    }

    if (m_target) {

        for (size_t ch = 0; ch < m_inputs.size(); ++ch) {
            inbufs[ch] = (float *)jack_port_get_buffer(m_inputs[ch], nframes);
        }

        peakLeft = 0.0; peakRight = 0.0;

        for (size_t ch = 0; ch < m_inputs.size(); ++ch) {

            float peak = 0.0;

            for (size_t i = 0; i < nframes; ++i) {
                float sample = fabsf(inbufs[ch][i]);
                if (sample > peak) peak = sample;
            }

            if (ch == 0) peakLeft = peak;
            if (ch > 0 || m_inputs.size() == 1) peakRight = peak;
        }
        
	m_target->setInputLevels(peakLeft, peakRight);
        m_target->putSamples(nframes, inbufs);

        /*!!!
        for (size_t ch = 0; ch < m_inputs.size(); ++ch) {
            for (size_t i = 0; i < nframes; ++i) {
                outbufs[ch][i] = inbufs[ch][i];
            }
        }
        */
    }

    m_mutex.unlock();
    return 0;
}

int
JACKAudioIO::xrun()
{
    std::cerr << "JACKAudioIO: xrun!" << std::endl;
    if (m_target) m_target->audioProcessingOverload();
    if (m_source) m_source->audioProcessingOverload();
    return 0;
}

}

#endif /* HAVE_JACK */

