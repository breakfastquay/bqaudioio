/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef BQAUDIOIO_SYSTEM_RECORD_SOURCE_H
#define BQAUDIOIO_SYSTEM_RECORD_SOURCE_H

#include "Suspendable.h"

namespace breakfastquay {

class ApplicationRecordTarget;

class SystemRecordSource : virtual public Suspendable
{
public:
    SystemRecordSource(ApplicationRecordTarget *target);
    virtual ~SystemRecordSource();

    virtual bool isSourceOK() const = 0;
    virtual bool isSourceReady() const { return isSourceOK(); }

protected:
    ApplicationRecordTarget *m_target;
};

}
#endif

