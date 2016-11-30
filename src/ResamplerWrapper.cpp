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

