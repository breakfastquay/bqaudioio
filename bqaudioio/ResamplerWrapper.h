/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    bqaudioio

    Copyright 2007-2016 Particular Programs Ltd.

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Except as contained in this notice, the names of Chris Cannam and
    Particular Programs Ltd shall not be used in advertising or
    otherwise to promote the sale, use or other dealings in this
    Software without prior written authorization.
*/

#ifndef RESAMPLER_WRAPPER_H
#define RESAMPLER_WRAPPER_H

#include "ApplicationPlaybackSource.h"

namespace breakfastquay {

class Resampler;

class ResamplerWrapper : public ApplicationPlaybackSource
{
public:
    ResamplerWrapper(ApplicationPlaybackSource *source);
    ~ResamplerWrapper();

    void changeApplicationSampleRate(int newRate);
    void reset(); // clear buffers
    
    virtual std::string getClientName() const;
    
    virtual int getApplicationSampleRate() const;
    virtual int getApplicationChannelCount() const;

    virtual void setSystemPlaybackBlockSize(int);
    virtual void setSystemPlaybackSampleRate(int);
    virtual void setSystemPlaybackChannelCount(int);
    virtual void setSystemPlaybackLatency(int);

    virtual int getSourceSamples(int nframes, float **samples);

    virtual void setOutputLevels(float peakLeft, float peakRight);
    virtual void audioProcessingOverload();

private:
    ApplicationPlaybackSource *m_source;
    
    int m_channels;
    int m_targetRate;
    int m_sourceRate;

    Resampler *m_resampler;

    float **m_in;
    int m_inSize;
    float **m_resampled;
    int m_resampledSize;
    int m_resampledFill;
    float **m_ptrs;

    void setupBuffersFor(int reqsize);
};

}

#endif
