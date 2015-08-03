/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef BQAUDIOIO_APPLICATION_RECORD_TARGET_H
#define BQAUDIOIO_APPLICATION_RECORD_TARGET_H

#include <stdlib.h>

namespace breakfastquay {

class ApplicationRecordTarget
{
public:
    virtual ~ApplicationRecordTarget() { }

    virtual int getApplicationSampleRate() const { return 0; }
    virtual int getApplicationChannelCount() const = 0; //!!! does this make sense at all? I think the target should have to deal with mismatches in channel count and sample rate
    
    virtual void setSystemRecordBlockSize(int) = 0;
    virtual void setSystemRecordSampleRate(int) = 0;
    virtual void setSystemRecordLatency(int) = 0;

    virtual void putSamples(int nframes, float **samples) = 0;
    
    virtual void setInputLevels(float peakLeft, float peakRight) = 0;

    virtual void audioProcessingOverload() { }
};

}
#endif
    
