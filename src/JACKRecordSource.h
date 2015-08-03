/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef _AUDIO_JACK_SOURCE_H_
#define _AUDIO_JACK_SOURCE_H_

#ifdef HAVE_JACK

#include <jack/jack.h>
#include <vector>

#include "SystemRecordSource.h"

#include <QMutex>

namespace breakfastquay {

class ApplicationRecordTarget;

class JACKRecordSource : public SystemRecordSource
{
public:
    JACKRecordSource(ApplicationRecordTarget *target);
    virtual ~JACKRecordSource();

    virtual bool isSourceOK() const;

protected:
    int process(jack_nframes_t nframes);
    int xrun();

    static int processStatic(jack_nframes_t, void *);
    static int xrunStatic(void *);

    jack_client_t              *m_client;
    std::vector<jack_port_t *>  m_inputs;
    jack_nframes_t              m_bufferSize;
    jack_nframes_t              m_sampleRate;
    QMutex                      m_mutex;
};

}

#endif /* HAVE_JACK */

#endif

