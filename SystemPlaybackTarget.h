/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef _AUDIO_CALLBACK_PLAY_TARGET_H_
#define _AUDIO_CALLBACK_PLAY_TARGET_H_

#include <QObject>

#include "base/TurbotTypes.h"

namespace Turbot {

class ApplicationPlaybackSource;

class SystemPlaybackTarget : public QObject
{
    Q_OBJECT

public:
    SystemPlaybackTarget(ApplicationPlaybackSource *source);
    virtual ~SystemPlaybackTarget();

    virtual bool isTargetOK() const = 0;
    virtual bool isTargetReady() const { return isTargetOK(); }

    virtual double getCurrentTime() const = 0;

    float getOutputGain() const {
	return m_outputGain;
    }

public slots:
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
