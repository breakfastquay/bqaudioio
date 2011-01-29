/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef _AUDIO_JACK_TARGET_H_
#define _AUDIO_JACK_TARGET_H_

#ifdef HAVE_JACK

#include <jack/jack.h>
#include <vector>

#include "SystemPlaybackTarget.h"

#include <QMutex>

namespace Turbot {

class ApplicationPlaybackSource;

class JACKPlaybackTarget : public SystemPlaybackTarget
{
public:
    JACKPlaybackTarget(ApplicationPlaybackSource *source);
    virtual ~JACKPlaybackTarget();

    virtual bool isTargetOK() const;

    virtual double getCurrentTime() const;

protected:
    void setup(size_t channels);
    int process(jack_nframes_t nframes);

    static int processStatic(jack_nframes_t, void *);

    jack_client_t              *m_client;
    std::vector<jack_port_t *>  m_outputs;
    jack_nframes_t              m_bufferSize;
    jack_nframes_t              m_sampleRate;
    QMutex                      m_mutex;
};

}

#endif /* HAVE_JACK */

#endif

