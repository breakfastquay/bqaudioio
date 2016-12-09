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

#include <iostream>

using namespace std;

namespace breakfastquay {

ResamplerWrapper::ResamplerWrapper(ApplicationPlaybackSource *source) :
    m_source(source),
    m_targetRate(44100), // will update when the target calls back
    m_sourceRate(0),
    m_resampler(0),
    m_in(0),
    m_inSize(0),
    m_resampled(0),
    m_resampledSize(0),
    m_resampledFill(0),
    m_ptrs(0)
{
    m_sourceRate = m_source->getApplicationSampleRate();

    // Note, m_sourceRate might be zero if the application is happy to
    // allow the device to be opened at any rate. We can't actually
    // work with a zero source rate, but the application may change it
    // through a call to changeApplicationSampleRate() before playback
    // begins, so we have to allow this at this point.
    
    m_channels = m_source->getApplicationChannelCount();
    m_resampler = new Resampler(Resampler::FastestTolerable, m_channels);
    m_ptrs = new float *[m_channels];
    setupBuffersFor(10240);
}

ResamplerWrapper::~ResamplerWrapper()
{
    delete m_resampler;
    delete[] m_ptrs;
    if (m_in) {
	deallocate_channels(m_in, m_channels);
	deallocate_channels(m_resampled, m_channels);
    }
}

void
ResamplerWrapper::changeApplicationSampleRate(int newRate)
{
    m_sourceRate = newRate;
    setupBuffersFor(10240);
}

std::string
ResamplerWrapper::getClientName() const
{
    return m_source->getClientName();
}

int
ResamplerWrapper::getApplicationSampleRate() const
{
    // Although we could return 0 here (as we can accept any rate from
    // the target), things are simplest if the target can offer the
    // rate that we actually do want. But this isn't supposed to
    // change, so call this source function rather than returning our
    // m_sourceRate (which is changeable)
    return m_source->getApplicationSampleRate();
}

int
ResamplerWrapper::getApplicationChannelCount() const
{
    return m_source->getApplicationChannelCount();
}

void
ResamplerWrapper::setSystemPlaybackBlockSize(int)
{
}

void
ResamplerWrapper::setSystemPlaybackSampleRate(int rate)
{
    m_targetRate = rate;
    m_source->setSystemPlaybackSampleRate(m_targetRate);
}

void
ResamplerWrapper::setSystemPlaybackChannelCount(int c)
{
    m_source->setSystemPlaybackChannelCount(c);
}

void
ResamplerWrapper::setSystemPlaybackLatency(int latency)
{
    m_source->setSystemPlaybackLatency(latency);
}

void
ResamplerWrapper::setOutputLevels(float left, float right)
{
    m_source->setOutputLevels(left, right);
}

void
ResamplerWrapper::audioProcessingOverload()
{
    m_source->audioProcessingOverload();
}

void
ResamplerWrapper::reset()
{
    if (m_resampler) m_resampler->reset();
    m_resampledFill = 0;
}

int
ResamplerWrapper::getSourceSamples(int nframes, float **samples)
{
    cerr << "ResamplerWrapper::getSourceSamples(" << nframes << "): source rate = " << m_sourceRate << ", target rate = " << m_targetRate << ", channels = " << m_channels << endl;
    
    setupBuffersFor(nframes);

    if (m_sourceRate == 0) {
        v_zero_channels(samples, m_channels, nframes);
        return nframes;
    }
    
    if (m_sourceRate == m_targetRate) {
	return m_source->getSourceSamples(nframes, samples);
    }

    double ratio = double(m_targetRate) / double(m_sourceRate);

    int reqResampled = nframes - m_resampledFill + 1;
    int req = int(round(reqResampled / ratio));
    int received = m_source->getSourceSamples(req, m_in);
    
    for (int i = 0; i < m_channels; ++i) {
        m_ptrs[i] = m_resampled[i] + m_resampledFill;
    }

    cerr << "ResamplerWrapper: nframes = " << nframes << ", ratio = " << ratio << endl;
    cerr << "ResamplerWrapper: m_inSize = " << m_inSize << ", m_resampledSize = "
         << m_resampledSize << ", m_resampledFill = " << m_resampledFill << endl;
    cerr << "ResamplerWrapper: reqResampled = " << reqResampled << ", req = "
         << req << ", received = " << received << endl;

    if (received > 0) {

        int resampled = m_resampler->resample
            (m_ptrs, m_resampledSize - m_resampledFill,
             m_in, received,
             ratio);

        m_resampledFill += resampled;

        cerr << "ResamplerWrapper: resampled = " << resampled << ", m_resampledFill now = " << m_resampledFill << endl;
    }
            
    if (m_resampledFill < nframes) {
	for (int i = 0; i < m_channels; ++i) {
	    v_zero(m_resampled[i] + m_resampledFill, nframes - m_resampledFill);
	}
        m_resampledFill = nframes;
    }

    v_copy_channels(samples, m_resampled, m_channels, nframes);

    if (m_resampledFill > nframes) {
        for (int i = 0; i < m_channels; ++i) {
            m_ptrs[i] = m_resampled[i] + nframes;
        }
        v_move_channels(m_resampled, m_ptrs, m_channels, m_resampledFill - nframes);
    }

    m_resampledFill -= nframes;

    cerr << "ResamplerWrapper: m_resampledFill now = " << m_resampledFill << " and returning nframes = " << nframes << endl;

    return nframes;
}

void
ResamplerWrapper::setupBuffersFor(int nframes)
{
    if (m_sourceRate == 0) return;
    if (m_sourceRate == m_targetRate) return;

    cerr << "ResamplerWrapper::setupBuffersFor: Source rate "
         << m_sourceRate << " -> target rate " << m_targetRate << endl;

    int slack = 100;
    double ratio = double(m_targetRate) / double(m_sourceRate);
    int newResampledSize = nframes + slack;
    int newInSize = int(newResampledSize / ratio);

    cerr << "newResampledSize = " << newResampledSize << ", newInSize = " << newInSize << endl;
    
    if (!m_resampled || newResampledSize > m_resampledSize) {
        m_resampled = reallocate_and_zero_extend_channels
            (m_resampled,
             m_channels, m_resampledSize,
             m_channels, newResampledSize);
        m_resampledSize = newResampledSize;
    }

    if (!m_in || newInSize > m_inSize) {
	m_in = reallocate_and_zero_extend_channels
            (m_in,
             m_channels, m_inSize,
             m_channels, newInSize);
	m_inSize = newInSize;
    }
}

}

