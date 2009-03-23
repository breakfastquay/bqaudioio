/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#include "AudioCallbackPlayTarget.h"

#include <iostream>

namespace Turbot {

AudioCallbackPlayTarget::AudioCallbackPlayTarget(AudioCallbackPlaySource *source) :
    m_source(source),
    m_outputGain(1.0)
{
}

AudioCallbackPlayTarget::~AudioCallbackPlayTarget()
{
}

void
AudioCallbackPlayTarget::setOutputGain(float gain)
{
    m_outputGain = gain;
}

}

