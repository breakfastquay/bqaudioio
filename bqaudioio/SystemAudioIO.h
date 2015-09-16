/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef BQAUDIOIO_SYSTEM_AUDIO_IO_H
#define BQAUDIOIO_SYSTEM_AUDIO_IO_H

#include "SystemRecordSource.h"
#include "SystemPlaybackTarget.h"

namespace breakfastquay {

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

    virtual void suspend() = 0;
    virtual void resume() = 0;
};

}

#endif
