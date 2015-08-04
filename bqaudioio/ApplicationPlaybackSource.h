/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef BQAUDIOIO_APPLICATION_PLAY_SOURCE_H
#define BQAUDIOIO_APPLICATION_PLAY_SOURCE_H

#include <string>

namespace breakfastquay {

class SystemPlaybackTarget;

class ApplicationPlaybackSource
{
public:
    virtual ~ApplicationPlaybackSource() { }

    virtual std::string getClientName() const = 0;
    
    virtual int getApplicationSampleRate() const = 0; // 0 for unknown or don't care //!!! doc this properly
    virtual int getApplicationChannelCount() const = 0;

    virtual void setSystemPlaybackBlockSize(int) = 0;
    virtual void setSystemPlaybackSampleRate(int) = 0;
    virtual void setSystemPlaybackLatency(int) = 0;

    virtual void getSourceSamples(int nframes, float **samples) = 0;
    
    virtual void setOutputLevels(float peakLeft, float peakRight) = 0;

    virtual void audioProcessingOverload() { }
};

}
#endif
    
