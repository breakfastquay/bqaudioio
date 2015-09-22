/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef BQAUDIOIO_PORTAUDIO_PLAYBACK_TARGET_H
#define BQAUDIOIO_PORTAUDIO_PLAYBACK_TARGET_H

#ifdef HAVE_PORTAUDIO

#include <portaudio.h>

#include "SystemPlaybackTarget.h"

namespace breakfastquay {

class ApplicationPlaybackSource;
class Resampler;

class PortAudioPlaybackTarget : public SystemPlaybackTarget
{
public:
    PortAudioPlaybackTarget(ApplicationPlaybackSource *source);
    virtual ~PortAudioPlaybackTarget();

    virtual bool isTargetOK() const;

    virtual double getCurrentTime() const;

    virtual void suspend();
    virtual void resume();
    
protected:
    int process(const void *input, void *output, unsigned long frames,
                const PaStreamCallbackTimeInfo *timeInfo,
                PaStreamCallbackFlags statusFlags);

    static int processStatic(const void *, void *, unsigned long,
                             const PaStreamCallbackTimeInfo *,
                             PaStreamCallbackFlags, void *);

    PaStream *m_stream;
    Resampler *m_resampler;
    int m_bufferSize;
    int m_sampleRate;
    int m_latency;
    bool m_suspended;
};

}

#endif /* HAVE_PORTAUDIO */

#endif

