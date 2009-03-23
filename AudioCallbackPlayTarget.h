/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifndef _AUDIO_CALLBACK_PLAY_TARGET_H_
#define _AUDIO_CALLBACK_PLAY_TARGET_H_

#include <QObject>

#include "base/Types.h"

namespace Turbot {

class AudioCallbackPlaySource;

class AudioCallbackPlayTarget : public QObject
{
    Q_OBJECT

public:
    AudioCallbackPlayTarget(AudioCallbackPlaySource *source);
    virtual ~AudioCallbackPlayTarget();

    virtual bool isTargetOK() const = 0;

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
    AudioCallbackPlaySource *m_source;
    float m_outputGain;
};

}

#endif

