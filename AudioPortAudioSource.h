/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef _AUDIO_PORT_AUDIO_SOURCE_H_
#define _AUDIO_PORT_AUDIO_SOURCE_H_

//!!!
#define HAVE_PORTAUDIO 1

#ifdef HAVE_PORTAUDIO

#include <portaudio.h>

#include "AudioCallbackRecordSource.h"

namespace Turbot {

class AudioCallbackRecordTarget;

class AudioPortAudioSource : public AudioCallbackRecordSource
{
public:
    AudioPortAudioSource(AudioCallbackRecordTarget *target);
    virtual ~AudioPortAudioSource();

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

