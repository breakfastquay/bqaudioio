/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef _AUDIO_CALLBACK_IO_H_
#define _AUDIO_CALLBACK_IO_H_

#include "SystemRecordSource.h"
#include "SystemPlaybackTarget.h"

#include "base/TurbotTypes.h"

namespace Turbot {

class SystemAudioIO : public SystemRecordSource,
			public SystemPlaybackTarget
{
public:
    SystemAudioIO(ApplicationRecordTarget *target,
                    ApplicationPlaybackSource *source) :
        SystemRecordSource(target),
        SystemPlaybackTarget(source) { }
};

}

#endif
