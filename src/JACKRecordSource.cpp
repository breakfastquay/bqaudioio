/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifdef HAVE_JACK

#include "JACKRecordSource.h"
#include "ApplicationRecordTarget.h"
#include "DynamicJACK.h"

#include <iostream>
#include <cmath>
#include <cstdio>

#include <unistd.h> // getpid

//#define DEBUG_AUDIO_JACK_SOURCE 1

namespace breakfastquay {

JACKRecordSource::JACKRecordSource(ApplicationRecordTarget *target) :
    SystemRecordSource(target),
    m_client(0),
    m_bufferSize(0),
    m_sampleRate(0)
{
    char name[100];
    strcpy(name, "turbot");
    m_client = jack_client_new(name);

    if (!m_client) {
	sprintf(name, "turbot (%d)", (int)getpid());
	m_client = jack_client_new(name);
	if (!m_client) {
	    std::cerr
		<< "ERROR: JACKRecordSource: Failed to connect to JACK server"
		<< std::endl;
	}
    }

    if (!m_client) return;

    m_bufferSize = jack_get_buffer_size(m_client);
    m_sampleRate = jack_get_sample_rate(m_client);

    jack_set_xrun_callback(m_client, xrunStatic, this);
    jack_set_process_callback(m_client, processStatic, this);

    if (jack_activate(m_client)) {
	std::cerr << "ERROR: JACKRecordSource: Failed to activate JACK client"
		  << std::endl;
    }

    m_target->setSystemRecordBlockSize(m_bufferSize);
    m_target->setSystemRecordSampleRate(m_sampleRate);

    int channels = m_target->getApplicationChannelCount();

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
		<< "ERROR: JACKRecordSource: Failed to create JACK input port "
		<< m_inputs.size() << std::endl;
	} else {
	    m_target->setSystemRecordLatency(jack_port_get_latency(port));
	}

	m_inputs.push_back(port);
    }
}

JACKRecordSource::~JACKRecordSource()
{
    if (m_client) {
	jack_deactivate(m_client);
	jack_client_close(m_client);
    }
}

bool
JACKRecordSource::isSourceOK() const
{
    return (m_client != 0);
}

int
JACKRecordSource::processStatic(jack_nframes_t nframes, void *arg)
{
    return ((JACKRecordSource *)arg)->process(nframes);
}

int
JACKRecordSource::xrunStatic(void *arg)
{
    return ((JACKRecordSource *)arg)->xrun();
}

int
JACKRecordSource::process(jack_nframes_t nframes)
{
    if (m_inputs.size() < m_target->getApplicationChannelCount()) {
	return 0;
    }

#ifdef DEBUG_AUDIO_JACK_SOURCE    
    std::cout << "JACKRecordSource::process(" << nframes << "), have " << m_inputs.size() << " inputs" << std::endl;
#endif

#ifdef DEBUG_AUDIO_JACK_SOURCE    
    if (m_bufferSize != nframes) {
	std::cerr << "WARNING: m_bufferSize != nframes (" << m_bufferSize << " != " << nframes << ")" << std::endl;
    }
#endif

    float **buffers = (float **)alloca(m_inputs.size() * sizeof(float *));

    for (int ch = 0; ch < m_inputs.size(); ++ch) {
	buffers[ch] = (float *)jack_port_get_buffer(m_inputs[ch], nframes);
    }

    if (m_target) {
	m_target->putSamples(nframes, buffers);
    }

    float peakLeft = 0.0, peakRight = 0.0;

    for (int ch = 0; ch < m_inputs.size(); ++ch) {

	float peak = 0.0;

	for (int i = 0; i < nframes; ++i) {
	    float sample = fabsf(buffers[ch][i]);
	    if (sample > peak) peak = sample;
	}

	if (ch == 0) peakLeft = peak;
	if (ch > 0 || m_inputs.size() == 1) peakRight = peak;
    }
	    
    if (m_target) {
	m_target->setInputLevels(peakLeft, peakRight);
    }

    return 0;
}

int
JACKRecordSource::xrun()
{
    std::cerr << "JACKRecordSource: xrun!" << std::endl;
    if (m_target) m_target->audioProcessingOverload();
    return 0;
}

}
#endif /* HAVE_JACK */

