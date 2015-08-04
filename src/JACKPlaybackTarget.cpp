/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifdef HAVE_JACK

#include "JACKPlaybackTarget.h"
#include "DynamicJACK.h"
#include "ApplicationPlaybackSource.h"

#include <iostream>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <unistd.h> // getpid

using namespace std;

namespace breakfastquay {

JACKPlaybackTarget::JACKPlaybackTarget(ApplicationPlaybackSource *source) :
    SystemPlaybackTarget(source),
    m_client(0),
    m_bufferSize(0),
    m_sampleRate(0)
{
    JackOptions options = JackNullOption;

#if defined(HAVE_PORTAUDIO) || defined(HAVE_LIBPULSE)
    options = JackNoStartServer;
#endif

    JackStatus status = JackStatus(0);
    m_client = jack_client_open(source->getClientName().c_str(),
                                options, &status);

    if (!m_client) {
        cerr << "ERROR: JACKPlaybackTarget: Failed to connect to JACK server"
             << endl;
        return;
    }

    m_bufferSize = jack_get_buffer_size(m_client);
    m_sampleRate = jack_get_sample_rate(m_client);

    jack_set_process_callback(m_client, processStatic, this);

    if (jack_activate(m_client)) {
	cerr << "ERROR: JACKPlaybackTarget: Failed to activate JACK client"
		  << endl;
    }

    setup(2);
}

JACKPlaybackTarget::~JACKPlaybackTarget()
{
    if (m_client) {
	jack_deactivate(m_client);
	jack_client_close(m_client);
    }
}

bool
JACKPlaybackTarget::isTargetOK() const
{
    return (m_client != 0);
}

double
JACKPlaybackTarget::getCurrentTime() const
{
    if (m_client && m_sampleRate) {
        return double(jack_frame_time(m_client)) / double(m_sampleRate);
    } else {
        return 0.0;
    }
}

int
JACKPlaybackTarget::processStatic(jack_nframes_t nframes, void *arg)
{
    return ((JACKPlaybackTarget *)arg)->process(nframes);
}

void
JACKPlaybackTarget::setup(int channels)
{
    lock_guard<mutex> guard(m_mutex);

    m_source->setSystemPlaybackBlockSize(m_bufferSize);
    m_source->setSystemPlaybackSampleRate(m_sampleRate);

    if (channels == m_outputs.size() || !m_client) {
	return;
    }

    const char **ports =
	jack_get_ports(m_client, NULL, NULL,
		       JackPortIsPhysical | JackPortIsInput);
    int physicalPortCount = 0;
    while (ports[physicalPortCount]) ++physicalPortCount;

#ifdef DEBUG_AUDIO_JACK_TARGET    
    cerr << "JACKPlaybackTarget::sourceModelReplaced: have " << channels << " channels and " << physicalPortCount << " physical ports" << endl;
#endif

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
	    cerr
		<< "ERROR: JACKPlaybackTarget: Failed to create JACK output port "
		<< m_outputs.size() << endl;
	} else {
            jack_latency_range_t range;
            jack_port_get_latency_range(port, JackPlaybackLatency, &range);
            m_source->setSystemPlaybackLatency(range.max);
	}

	if (m_outputs.size() < physicalPortCount) {
	    jack_connect(m_client, jack_port_name(port), ports[m_outputs.size()]);
	}

	m_outputs.push_back(port);
    }

    while (m_outputs.size() > channels) {
	vector<jack_port_t *>::iterator itr = m_outputs.end();
	--itr;
	jack_port_t *port = *itr;
	if (port) jack_port_unregister(m_client, port);
	m_outputs.erase(itr);
    }
}

int
JACKPlaybackTarget::process(jack_nframes_t nframes)
{
    if (!m_mutex.try_lock()) {
	return 0;
    }

    lock_guard<mutex> guard(m_mutex, adopt_lock);
    
    if (m_outputs.empty()) {
	return 0;
    }

#ifdef DEBUG_AUDIO_JACK_TARGET    
    cout << "JACKPlaybackTarget::process(" << nframes << "): have a source" << endl;
#endif

#ifdef DEBUG_AUDIO_JACK_TARGET    
    if (m_bufferSize != nframes) {
	cerr << "WARNING: m_bufferSize != nframes (" << m_bufferSize << " != " << nframes << ")" << endl;
    }
#endif

    float **buffers = (float **)alloca(m_outputs.size() * sizeof(float *));

    for (int ch = 0; ch < m_outputs.size(); ++ch) {
	buffers[ch] = (float *)jack_port_get_buffer(m_outputs[ch], nframes);
    }

    if (m_source) {
	m_source->getSourceSamples(nframes, buffers);
    } else {
	for (int ch = 0; ch < m_outputs.size(); ++ch) {
	    for (int i = 0; i < nframes; ++i) {
		buffers[ch][i] = 0.0;
	    }
	}
    }

    float peakLeft = 0.0, peakRight = 0.0;

    for (int ch = 0; ch < m_outputs.size(); ++ch) {

	float peak = 0.0;

	for (int i = 0; i < nframes; ++i) {
	    buffers[ch][i] *= m_outputGain;
	    float sample = fabsf(buffers[ch][i]);
	    if (sample > peak) peak = sample;
	}

	if (ch == 0) peakLeft = peak;
	if (ch > 0 || m_outputs.size() == 1) peakRight = peak;
    }
	    
    if (m_source) {
	m_source->setOutputLevels(peakLeft, peakRight);
    }

    return 0;
}

}

#endif /* HAVE_JACK */

