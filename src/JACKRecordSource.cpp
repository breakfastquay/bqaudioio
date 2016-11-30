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

#ifdef HAVE_JACK

#include "JACKRecordSource.h"
#include "ApplicationRecordTarget.h"
#include "DynamicJACK.h"

#include <bqvec/Range.h>

#include <iostream>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <climits>

#include <unistd.h> // getpid

//#define DEBUG_AUDIO_JACK_SOURCE 1

using namespace std;

namespace breakfastquay {

JACKRecordSource::JACKRecordSource(ApplicationRecordTarget *target) :
    SystemRecordSource(target),
    m_client(0),
    m_bufferSize(0),
    m_sampleRate(0)
{
    JackOptions options = JackNullOption;

#if defined(HAVE_PORTAUDIO) || defined(HAVE_LIBPULSE)
    options = JackNoStartServer;
#endif

    JackStatus status = JackStatus(0);
    m_client = jack_client_open(target->getClientName().c_str(),
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
	cerr << "ERROR: JACKRecordSource: Failed to activate JACK client"
		  << endl;
    }

    m_target->setSystemRecordBlockSize(m_bufferSize);
    m_target->setSystemRecordSampleRate(m_sampleRate);

    int channels = m_target->getApplicationChannelCount();

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
	    cerr
		<< "ERROR: JACKRecordSource: Failed to create JACK input port "
		<< m_inputs.size() << endl;
	} else {
            jack_latency_range_t range;
            jack_port_get_latency_range(port, JackCaptureLatency, &range);
            m_target->setSystemRecordLatency(range.max);
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
JACKRecordSource::process(jack_nframes_t j_nframes)
{
    if (int(m_inputs.size()) < m_target->getApplicationChannelCount()) {
	return 0;
    }

    if (j_nframes > INT_MAX) j_nframes = 0;
    int nframes = int(j_nframes);
    
#ifdef DEBUG_AUDIO_JACK_SOURCE    
    cout << "JACKRecordSource::process(" << nframes << "), have " << m_inputs.size() << " inputs" << endl;
#endif

#ifdef DEBUG_AUDIO_JACK_SOURCE    
    if (m_bufferSize != nframes) {
	cerr << "WARNING: m_bufferSize != nframes (" << m_bufferSize << " != " << nframes << ")" << endl;
    }
#endif

    float **buffers = (float **)alloca(m_inputs.size() * sizeof(float *));

    for (int ch = 0; in_range_for(m_inputs, ch); ++ch) {
	buffers[ch] = (float *)jack_port_get_buffer(m_inputs[ch], nframes);
    }

    if (m_target) {
	m_target->putSamples(nframes, buffers);
    }

    float peakLeft = 0.0, peakRight = 0.0;

    for (int ch = 0; in_range_for(m_inputs, ch); ++ch) {

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
    cerr << "JACKRecordSource: xrun!" << endl;
    if (m_target) m_target->audioProcessingOverload();
    return 0;
}

}
#endif /* HAVE_JACK */

