/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifndef _AUDIO_CALLBACK_PLAY_SOURCE_H_
#define _AUDIO_CALLBACK_PLAY_SOURCE_H_

#include <stdlib.h>

#include "base/TurbotTypes.h"

namespace Turbot {

class AudioCallbackPlaySource
{
public:
    virtual ~AudioCallbackPlaySource() { }

    virtual size_t getSourceSampleRate() = 0; // 0 for unknown or don't care
    virtual size_t getSourceChannelCount() = 0;

    virtual long getCurrentPlayingFrame() const = 0;
    
    virtual void setTargetBlockSize(size_t) = 0;
    virtual void setTargetSampleRate(size_t) = 0;
    virtual void setTargetPlayLatency(size_t) = 0;

    virtual void getSourceSamples(size_t nframes, float **samples) = 0;
    
    virtual void setOutputLevels(float peakLeft, float peakRight) = 0;

    virtual void audioProcessingOverload() { }
};

}
#endif
    
