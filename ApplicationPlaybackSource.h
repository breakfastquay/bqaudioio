/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef _AUDIO_CALLBACK_PLAY_SOURCE_H_
#define _AUDIO_CALLBACK_PLAY_SOURCE_H_

#include <stdlib.h>

#include "base/TurbotTypes.h"

namespace Turbot {

class ApplicationPlaybackSource
{
public:
    virtual ~ApplicationPlaybackSource() { }

    virtual int getApplicationSampleRate() = 0; // 0 for unknown or don't care
    virtual int getApplicationChannelCount() = 0;

    virtual long getCurrentPlayingFrame() const = 0;
    
    virtual void setSystemPlaybackBlockSize(int) = 0;
    virtual void setSystemPlaybackSampleRate(int) = 0;
    virtual void setSystemPlaybackLatency(int) = 0;

    virtual void getSourceSamples(int nframes, float **samples) = 0;
    
    virtual void setOutputLevels(float peakLeft, float peakRight) = 0;

    virtual void audioProcessingOverload() { }
};

}
#endif
    
