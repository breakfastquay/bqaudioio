/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef _AUDIO_PORT_AUDIO_SOURCE_H_
#define _AUDIO_PORT_AUDIO_SOURCE_H_

#ifdef HAVE_PORTAUDIO

#include <portaudio.h>

#include "SystemRecordSource.h"

namespace Turbot {

class ApplicationRecordTarget;

class PortAudioRecordSource : public SystemRecordSource
{
public:
    PortAudioRecordSource(ApplicationRecordTarget *target);
    virtual ~PortAudioRecordSource();

    virtual bool isSourceOK() const;

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
    int m_latency;
};

}

#endif /* HAVE_PORTAUDIO */

#endif

