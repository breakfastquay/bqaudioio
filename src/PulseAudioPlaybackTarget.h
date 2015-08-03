/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef _AUDIO_PULSE_AUDIO_PLAYBACK_TARGET_H_
#define _AUDIO_PULSE_AUDIO_PLAYBACK_TARGET_H_

#ifdef HAVE_LIBPULSE

#include <pulse/pulseaudio.h>

#include "SystemPlaybackTarget.h"

#include "system/Thread.h"

#include <QMutex>

namespace breakfastquay {

class ApplicationPlaybackSource;

class PulseAudioPlaybackTarget : public SystemPlaybackTarget
{
public:
    PulseAudioPlaybackTarget(ApplicationPlaybackSource *playSource);
    virtual ~PulseAudioPlaybackTarget();

    virtual bool isTargetOK() const;

    virtual double getCurrentTime() const;

protected:
    void streamWrite(int);
    void streamStateChanged(pa_stream *);
    void contextStateChanged();

    static void streamWriteStatic(pa_stream *, size_t, void *);
    static void streamStateChangedStatic(pa_stream *, void *);
    static void streamOverflowStatic(pa_stream *, void *);
    static void streamUnderflowStatic(pa_stream *, void *);
    static void contextStateChangedStatic(pa_context *, void *);

    QMutex m_mutex;

    class MainLoopThread : public Thread
    {
    public:
        MainLoopThread(pa_mainloop *loop) : m_loop(loop) { } //!!! RT???
        virtual void run() {
            int rv = 0;
            pa_mainloop_run(m_loop, &rv); //!!! check return value from this, and rv
        }

    private:
        pa_mainloop *m_loop;
    };

    pa_mainloop *m_loop;
    pa_mainloop_api *m_api;
    pa_context *m_context;
    pa_stream *m_out;
    pa_sample_spec m_spec;

    MainLoopThread *m_loopThread;

    int m_bufferSize;
    int m_sampleRate;
    int m_paChannels;
    bool m_done;

    bool m_playbackReady;
};

}

#endif /* HAVE_LIBPULSE */

#endif

