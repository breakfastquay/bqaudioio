/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef _AUDIO_PORT_AUDIO_IO_H_
#define _AUDIO_PORT_AUDIO_IO_H_

#ifdef HAVE_PORTAUDIO

#include <portaudio.h>

#include "SystemAudioIO.h"

namespace breakfastquay {

class ApplicationRecordTarget;
class ApplicationPlaybackSource;

class PortAudioIO : public SystemAudioIO
{
public:
    PortAudioIO(ApplicationRecordTarget *recordTarget,
                     ApplicationPlaybackSource *playSource);
    virtual ~PortAudioIO();

    virtual bool isSourceOK() const;
    virtual bool isTargetOK() const;

    virtual void suspend();
    virtual void resume();
    
    virtual double getCurrentTime() const;

protected:
    int process(const void *input, void *output, unsigned long frames,
                const PaStreamCallbackTimeInfo *timeInfo,
                PaStreamCallbackFlags statusFlags);

    static int processStatic(const void *, void *, unsigned long,
                             const PaStreamCallbackTimeInfo *,
                             PaStreamCallbackFlags, void *);

    PaStream *m_stream;

    int m_bufferSize;
    int m_sampleRate;
    int m_inputLatency;
    int m_outputLatency;
    bool m_prioritySet;
    bool m_suspended;
};

}

#endif /* HAVE_PORTAUDIO */

#endif

