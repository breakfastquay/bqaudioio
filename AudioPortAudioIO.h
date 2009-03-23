/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifndef _AUDIO_PORT_AUDIO_IO_H_
#define _AUDIO_PORT_AUDIO_IO_H_

#ifdef HAVE_PORTAUDIO

#include <portaudio.h>

#include "AudioCallbackIO.h"

namespace Turbot {

class AudioCallbackRecordTarget;
class AudioCallbackPlaySource;

class AudioPortAudioIO : public AudioCallbackIO
{
public:
    AudioPortAudioIO(AudioCallbackRecordTarget *recordTarget,
                     AudioCallbackPlaySource *playSource);
    virtual ~AudioPortAudioIO();

    virtual bool isSourceOK() const;
    virtual bool isTargetOK() const;

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
    int m_outputLatency;
    int m_inputLatency;
};

}

#endif /* HAVE_PORTAUDIO */

#endif

