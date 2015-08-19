/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef RESAMPLER_WRAPPER_H
#define RESAMPLER_WRAPPER_H

namespace breakfastquay {

class ApplicationPlaybackSource;
class Resampler;

class ResamplerWrapper
{
public:
    ResamplerWrapper(ApplicationPlaybackSource *source, int targetRate);
    ~ResamplerWrapper();

    int getSourceSamples(int nframes, float **samples);

private:
    ApplicationPlaybackSource *m_source;
    
    int m_channels;
    int m_targetRate;
    int m_sourceRate;

    Resampler *m_resampler;

    float **m_in;
    int m_insize;

    void setupBuffersFor(int reqsize);
};

}

#endif
