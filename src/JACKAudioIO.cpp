/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifdef HAVE_JACK

#include "JACKAudioIO.h"
#include "DynamicJACK.h"
#include "ApplicationPlaybackSource.h"
#include "ApplicationRecordTarget.h"

#include <bqvec/Range.h>

#include <iostream>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <climits>

#include <unistd.h> // getpid

//#define DEBUG_AUDIO_JACK_IO 1

using namespace std;

namespace breakfastquay {

JACKAudioIO::JACKAudioIO(ApplicationRecordTarget *target,
			 ApplicationPlaybackSource *source) :
    SystemAudioIO(target, source),
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

    jack_set_xrun_callback(m_client, xrunStatic, this);
    jack_set_process_callback(m_client, processStatic, this);

    if (jack_activate(m_client)) {
	cerr << "ERROR: JACKAudioIO: Failed to activate JACK client"
		  << endl;
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
JACKAudioIO::setup(int channels)
{
    lock_guard<mutex> guard(m_mutex);

    if (m_source) {
        m_source->setSystemPlaybackBlockSize(m_bufferSize);
        m_source->setSystemPlaybackSampleRate(m_sampleRate);
    }
    if (m_target) {
        m_target->setSystemRecordBlockSize(m_bufferSize);
        m_target->setSystemRecordSampleRate(m_sampleRate);
        if (channels < m_target->getApplicationChannelCount()) {
            channels = m_target->getApplicationChannelCount();
        }
    }

    if (channels == int(m_outputs.size()) || !m_client) {
	return;
    }

    const char **playPorts =
	jack_get_ports(m_client, NULL, NULL,
		       JackPortIsPhysical | JackPortIsInput);
    const char **capPorts =
	jack_get_ports(m_client, NULL, NULL,
		       JackPortIsPhysical | JackPortIsOutput);

    int playPortCount = 0;
    while (playPorts && playPorts[playPortCount]) ++playPortCount;

    int capPortCount = 0;
    while (capPorts && capPorts[capPortCount]) ++capPortCount;

#ifdef DEBUG_AUDIO_JACK_IO    
    cerr << "JACKAudioIO::setup: have " << channels << " channels, " << capPortCount << " capture ports, " << playPortCount << " playback ports" << endl;
#endif

    if (m_source) {

        while (int(m_outputs.size()) < channels) {
	
            char name[20];
            jack_port_t *port;

            sprintf(name, "out %ld", long(m_outputs.size() + 1));

            port = jack_port_register(m_client,
                                      name,
                                      JACK_DEFAULT_AUDIO_TYPE,
                                      JackPortIsOutput,
                                      0);

            if (!port) {
                cerr << "ERROR: JACKAudioIO: Failed to create JACK output port "
                    << m_outputs.size() << endl;
                return;
            } else {
                jack_latency_range_t range;
                jack_port_get_latency_range(port, JackPlaybackLatency, &range);
                m_source->setSystemPlaybackLatency(range.max);
            }

            if (int(m_outputs.size()) < playPortCount) {
                jack_connect(m_client,
                             jack_port_name(port),
                             playPorts[m_outputs.size()]);
            }

            m_outputs.push_back(port);
        }
    }

    if (m_target) {

        while (int(m_inputs.size()) < channels) {
	
            char name[20];
            jack_port_t *port;

            sprintf(name, "in %ld", long(m_inputs.size() + 1));

            port = jack_port_register(m_client,
                                      name,
                                      JACK_DEFAULT_AUDIO_TYPE,
                                      JackPortIsInput,
                                      0);

            if (!port) {
                cerr << "ERROR: JACKAudioIO: Failed to create JACK input port "
                    << m_inputs.size() << endl;
                return;
            } else {
                jack_latency_range_t range;
                jack_port_get_latency_range(port, JackCaptureLatency, &range);
                m_target->setSystemRecordLatency(range.max);
            }

            if (int(m_inputs.size()) < capPortCount) {
                jack_connect(m_client,
                             capPorts[m_inputs.size()],
                             jack_port_name(port));
            }

            m_inputs.push_back(port);
        }
    }

    while (int(m_outputs.size()) > channels) {
	vector<jack_port_t *>::iterator itr = m_outputs.end();
	--itr;
	jack_port_t *port = *itr;
	if (port) jack_port_unregister(m_client, port);
	m_outputs.erase(itr);
    }

    while (int(m_inputs.size()) > channels) {
	vector<jack_port_t *>::iterator itr = m_inputs.end();
	--itr;
	jack_port_t *port = *itr;
	if (port) jack_port_unregister(m_client, port);
	m_inputs.erase(itr);
    }
}

int
JACKAudioIO::process(jack_nframes_t j_nframes)
{
    if (!m_mutex.try_lock()) {
	return 0;
    }

    lock_guard<mutex> guard(m_mutex, adopt_lock);

    if (j_nframes > INT_MAX) j_nframes = 0;
    int nframes = int(j_nframes);
    
    if (m_outputs.empty() && m_inputs.empty()) {
	return 0;
    }

#ifdef DEBUG_AUDIO_JACK_IO    
    cout << "JACKAudioIO::process(" << nframes << "): have a purpose in life" << endl;
#endif

#ifdef DEBUG_AUDIO_JACK_IO    
    if (m_bufferSize != nframes) {
	cerr << "WARNING: m_bufferSize != nframes (" << m_bufferSize << " != " << nframes << ")" << endl;
    }
#endif

    float **inbufs = (float **)alloca(m_inputs.size() * sizeof(float *));
    float **outbufs = (float **)alloca(m_outputs.size() * sizeof(float *));

    float peakLeft, peakRight;

    if (m_source) {

        for (int ch = 0; in_range_for(m_outputs, ch); ++ch) {
            outbufs[ch] = (float *)jack_port_get_buffer(m_outputs[ch], nframes);
        }
        
	m_source->getSourceSamples(nframes, outbufs);

        peakLeft = 0.0; peakRight = 0.0;

        //!!! vectorisation

        for (int ch = 0; in_range_for(m_outputs, ch); ++ch) {

            float peak = 0.0;

            for (int i = 0; i < nframes; ++i) {
                outbufs[ch][i] *= m_outputGain;
                float sample = fabsf(outbufs[ch][i]);
                if (sample > peak) peak = sample;
            }

            if (ch == 0) peakLeft = peak;
            if (ch > 0 || m_outputs.size() == 1) peakRight = peak;
        }
	    
        m_source->setOutputLevels(peakLeft, peakRight);

    } else {
	for (int ch = 0; in_range_for(m_outputs, ch); ++ch) {
	    for (int i = 0; i < nframes; ++i) {
		outbufs[ch][i] = 0.0;
	    }
	}
    }

    if (m_target) {

        for (int ch = 0; in_range_for(m_inputs, ch); ++ch) {
            inbufs[ch] = (float *)jack_port_get_buffer(m_inputs[ch], nframes);
        }

        peakLeft = 0.0; peakRight = 0.0;

        for (int ch = 0; in_range_for(m_inputs, ch); ++ch) {

            float peak = 0.0;

            for (int i = 0; i < nframes; ++i) {
                float sample = fabsf(inbufs[ch][i]);
                if (sample > peak) peak = sample;
            }

            if (ch == 0) peakLeft = peak;
            if (ch > 0 || m_inputs.size() == 1) peakRight = peak;
        }
        
	m_target->setInputLevels(peakLeft, peakRight);
        m_target->putSamples(nframes, inbufs);
    }

    return 0;
}

int
JACKAudioIO::xrun()
{
    cerr << "JACKAudioIO: xrun!" << endl;
    if (m_target) m_target->audioProcessingOverload();
    if (m_source) m_source->audioProcessingOverload();
    return 0;
}

}

#endif /* HAVE_JACK */

