/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef BQAUDIOIO_SYSTEM_PLAYBACK_TARGET_H
#define BQAUDIOIO_SYSTEM_PLAYBACK_TARGET_H

#include "Suspendable.h"

namespace breakfastquay {

class ApplicationPlaybackSource;

class SystemPlaybackTarget : virtual public Suspendable
{
public:
    SystemPlaybackTarget(ApplicationPlaybackSource *source);
    virtual ~SystemPlaybackTarget();

    virtual bool isTargetOK() const = 0;
    virtual bool isTargetReady() const { return isTargetOK(); }

    virtual double getCurrentTime() const = 0;

    float getOutputGain() const {
	return m_outputGain;
    }

    /**
     * Set the playback gain (0.0 = silence, 1.0 = levels unmodified)
     */
    virtual void setOutputGain(float gain);

protected:
    ApplicationPlaybackSource *m_source;
    float m_outputGain;
};

}

#endif

