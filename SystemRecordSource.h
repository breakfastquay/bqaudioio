/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef _AUDIO_CALLBACK_RECORD_SOURCE_H_
#define _AUDIO_CALLBACK_RECORD_SOURCE_H_

#include <QObject>

namespace Turbot {

class ApplicationRecordTarget;

class SystemRecordSource
{
public:
    SystemRecordSource(ApplicationRecordTarget *target);
    virtual ~SystemRecordSource();

    virtual bool isSourceOK() const = 0;

protected:
    ApplicationRecordTarget *m_target;
};

}
#endif

