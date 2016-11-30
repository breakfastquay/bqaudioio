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

