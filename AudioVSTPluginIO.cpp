/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifdef HAVE_VST

#include "AudioVSTPluginIO.h"

#include <vst2.x/audioeffect.h>

#include <iostream>
#include <exception>

namespace Turbot {

AudioVSTPlugin::AudioVSTPlugin(audioMasterCallback cb) :
    AudioEffect(cb, 0, 0),
    m_sampleRate(0),
    m_blockSize(0),
    m_io(0),
    m_gui(0),
    m_guiConstructor(0)
{
    setNumInputs(0);
    setNumOutputs(2);
    canProcessReplacing(true);
    canDoubleReplacing(false);

    // subclass must call setUniqueID

    // m_io is created by subclass
}

~AudioVSTPlugin()
{
    delete m_io;
}

void
AudioVSTPlugin::registerGUIConstructor(PluginGUIConstructor *p) { m_guiConstructor = w; }

void
AudioVSTPlugin::getVendorString(char *n) 
{
    // max here is 64 chars
    vst_strncpy(n, "Breakfast Quay", kVstMaxVendorStrLen);
}

void
AudioVSTPlugin::setSampleRate (float sampleRate)
{
    m_sampleRate = sampleRate;
}

void
AudioVSTPlugin::setBlockSize(VstInt32 blockSize)
{
    //!!! what if we already started processing?
    m_blockSize = blockSize;
}
    

AudioVSTPluginIO::AudioVSTPluginIO(AudioCallbackRecordTarget *recordTarget,
				   AudioCallbackPlaySource *playSource,
                                   AudioVSTPlugin *plugin) :
    AudioCallbackIO(recordTarget, playSource),
    m_plugin(plugin)
{
/*
    try {
	m_plugin = new Plugin(this, cb);
    } catch (std::exception &e) {
	std::cerr << "AudioVSTPluginIO: Failed to initialise plugin: " << e.what() << std::endl;
	delete m_plugin;
	m_plugin = 0;
    }
*/
}

AudioVSTPluginIO::~AudioVSTPluginIO()
{
//    delete m_plugin; //!!! no, it owns me
}

bool
AudioVSTPluginIO::isSourceOK() const
{
    return (m_plugin != 0); //!!! no, source is something else
}

bool
AudioVSTPluginIO::isTargetOK() const
{
    return (m_plugin && m_plugin->isOK());
}

void
AudioVSTPluginIO::registerGui(QWidget *w)
{
    if (m_plugin) m_plugin->registerGui(w);
}

}

#endif
