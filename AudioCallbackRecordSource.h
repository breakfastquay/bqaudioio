/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef _AUDIO_CALLBACK_RECORD_SOURCE_H_
#define _AUDIO_CALLBACK_RECORD_SOURCE_H_

#include <QObject>

namespace Turbot {

class AudioCallbackRecordTarget;

class AudioCallbackRecordSource
{
public:
    AudioCallbackRecordSource(AudioCallbackRecordTarget *target);
    virtual ~AudioCallbackRecordSource();

    virtual bool isSourceOK() const = 0;

protected:
    AudioCallbackRecordTarget *m_target;
};

}
#endif

