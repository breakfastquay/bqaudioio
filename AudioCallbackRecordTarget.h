/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef _AUDIO_CALLBACK_RECORD_TARGET_H_
#define _AUDIO_CALLBACK_RECORD_TARGET_H_

#include <stdlib.h>

#include "base/TurbotTypes.h"

namespace Turbot {

class AudioCallbackRecordTarget
{
public:
    virtual ~AudioCallbackRecordTarget() { }

    virtual size_t getPreferredSampleRate() const { return 0; }
    virtual size_t getChannelCount() const = 0;
    
    virtual void setSourceBlockSize(size_t) = 0;
    virtual void setSourceSampleRate(size_t) = 0;
    virtual void setSourceRecordLatency(size_t) = 0;

    virtual void putSamples(size_t nframes, float **samples) = 0;
    
    virtual void setInputLevels(float peakLeft, float peakRight) = 0;

    virtual void audioProcessingOverload() { }
};

}
#endif
    
