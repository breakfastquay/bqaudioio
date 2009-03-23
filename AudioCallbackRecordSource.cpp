/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#include "AudioCallbackRecordSource.h"
#include "AudioCallbackRecordTarget.h"

#include <iostream>

namespace Turbot {

AudioCallbackRecordSource::AudioCallbackRecordSource(AudioCallbackRecordTarget *target) :
    m_target(target)
{
}

AudioCallbackRecordSource::~AudioCallbackRecordSource()
{
}

}
