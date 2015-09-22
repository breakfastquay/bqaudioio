/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef BQAUDIOIO_JACK_TARGET_H_
#define BQAUDIOIO_JACK_TARGET_H_

#ifdef HAVE_JACK

#include <jack/jack.h>
#include <vector>
#include <mutex>

#include "SystemPlaybackTarget.h"

namespace breakfastquay {

class ApplicationPlaybackSource;

class JACKPlaybackTarget : public SystemPlaybackTarget
{
public:
    JACKPlaybackTarget(ApplicationPlaybackSource *source);
    virtual ~JACKPlaybackTarget();

    virtual bool isTargetOK() const;

    virtual double getCurrentTime() const;

    virtual void suspend() {}
    virtual void resume() {}

protected:
    void setup(int channels);
    int process(jack_nframes_t nframes);

    static int processStatic(jack_nframes_t, void *);

    jack_client_t              *m_client;
    std::vector<jack_port_t *>  m_outputs;
    jack_nframes_t              m_bufferSize;
    jack_nframes_t              m_sampleRate;
    std::mutex                  m_mutex;
};

}

#endif /* HAVE_JACK */

#endif

