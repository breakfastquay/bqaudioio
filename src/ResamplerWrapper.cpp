/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#include "ResamplerWrapper.h"

#include "bqresample/Resampler.h"
#include "bqvec/Allocators.h"
#include "bqvec/VectorOps.h"

#include "ApplicationPlaybackSource.h"

using namespace std;

namespace breakfastquay {

ResamplerWrapper::ResamplerWrapper(ApplicationPlaybackSource *source,
				   int targetRate) :
    m_source(source),
    m_targetRate(targetRate),
    m_sourceRate(0),
    m_resampler(0),
    m_in(0),
    m_insize(0)
{
    m_channels = m_source->getApplicationChannelCount();
    m_resampler = new Resampler(Resampler::FastestTolerable, m_channels);
    setupBuffersFor(10240);
}

ResamplerWrapper::~ResamplerWrapper()
{
    delete m_resampler;
    if (m_in) {
	deallocate_channels(m_in, m_channels);
    }
}

void
ResamplerWrapper::setupBuffersFor(int reqsize)
{
    m_sourceRate = m_source->getApplicationSampleRate();
    if (m_sourceRate == 0) m_sourceRate = m_targetRate;
    if (m_sourceRate == m_targetRate) return;

    double ratio = double(m_targetRate) / double(m_sourceRate);
    int insize = int(reqsize / ratio) + 1;
    if (insize > m_insize) {
	reallocate_and_zero_extend_channels(m_in,
					    m_channels, m_insize,
					    m_channels, insize);
	m_insize = insize;
    }
}

int
ResamplerWrapper::getSourceSamples(int nframes, float **samples)
{
    setupBuffersFor(nframes);

    if (m_sourceRate == m_targetRate) {
	return m_source->getSourceSamples(nframes, samples);
    }

    //!!! what if source channels changes?

    double ratio = double(m_targetRate) / double(m_sourceRate);

    int req = int(nframes / ratio);
    int received = m_source->getSourceSamples(req, m_in);

    if (received < req) {
	for (int i = 0; i < m_channels; ++i) {
	    v_zero(m_in + received, req - received);
	}
    }

    //!!! todo: manage the 1-sample rounding errors
    
    return m_resampler->resample(m_in, samples, req, ratio);
}

}

