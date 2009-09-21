/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifdef HAVE_VST

#include "AudioVSTPluginIO.h"

#include <vst2.x/audioeffect.h>

#include <iostream>
#include <exception>

namespace Turbot {

class AudioVSTPluginIO::Plugin : public AudioEffect
{
public:
    Plugin(AudioVSTPluginIO *io, audioMasterCallback cb) :
	AudioEffect(cb, 0, 0),
	m_io(io) {
    }
    ~Plugin() { }

};
    

AudioVSTPluginIO::AudioVSTPluginIO(AudioCallbackRecordTarget *recordTarget,
				   AudioCallbackPlaySource *playSource) :
    AudioCallbackIO(recordTarget, playSource),
    m_plugin(0)
{
    try {
	m_plugin = new Plugin(this, cb); //!!!
    } catch (std::exception &e) {
	std::cerr << "AudioVSTPluginIO: Failed to initialise plugin: " << e.what() << std::endl;
	delete m_plugin;
	m_plugin = 0;
    }
}

AudioVSTPluginIO::~AudioVSTPluginIO()
{
    delete m_plugin;
}

bool
AudioVSTPluginIO::isSourceOK() const
{
    return (m_plugin != 0);
}

bool
AudioVSTPluginIO::isTargetOK() const
{
    return (m_plugin != 0);
}

}

#endif
