/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#include "SystemRecordSource.h"
#include "ApplicationRecordTarget.h"

#include <iostream>

namespace Turbot {

SystemRecordSource::SystemRecordSource(ApplicationRecordTarget *target) :
    m_target(target)
{
}

SystemRecordSource::~SystemRecordSource()
{
}

}
