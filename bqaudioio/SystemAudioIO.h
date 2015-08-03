/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef BQAUDIOIO_SYSTEM_AUDIO_IO_H
#define BQAUDIOIO_SYSTEM_AUDIO_IO_H

#include "SystemRecordSource.h"
#include "SystemPlaybackTarget.h"

namespace Turbot {

class SystemAudioIO : public SystemRecordSource,
                      public SystemPlaybackTarget
{
public:
    SystemAudioIO(ApplicationRecordTarget *target,
                  ApplicationPlaybackSource *source) :
        SystemRecordSource(target),
        SystemPlaybackTarget(source) { }

    bool isOK() const { return isSourceOK() && isTargetOK(); }
    bool isReady() const { return isSourceReady() && isTargetReady(); }
};

}

#endif
