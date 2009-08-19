/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef _AUDIO_CALLBACK_IO_H_
#define _AUDIO_CALLBACK_IO_H_

#include "AudioCallbackRecordSource.h"
#include "AudioCallbackPlayTarget.h"

#include "base/TurbotTypes.h"

namespace Turbot {

class AudioCallbackIO : public AudioCallbackRecordSource,
			public AudioCallbackPlayTarget
{
public:
    AudioCallbackIO(AudioCallbackRecordTarget *target,
                    AudioCallbackPlaySource *source) :
        AudioCallbackRecordSource(target),
        AudioCallbackPlayTarget(source) { }
};

}

#endif
