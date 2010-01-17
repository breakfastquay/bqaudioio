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
	m_io(io),
        m_gui(0),
        m_guiConstructor(0)
    {
        //!!! obviously, needs to vary from one plugin to the next!

        // NB the id "tbtR" has been registered at
        // service.steinberg.de/databases/plugin.nsf/plugIn?openForm
        // as the ID for "Rubber Band Audio Processor"

        setUniqueID("tbtR");

        setNumInputs(0);
        setNumOutputs(2);
        canProcessReplacing(true);
        canDoubleReplacing(false);
    }

    ~Plugin() { }

    void registerGUIConstructor(PluginGUIConstructor *p) { m_guiConstructor = w; }

    virtual void open () {}	///< Called when plug-in is initialized
    virtual void close () {}	///< Called when plug-in will be released
    virtual void suspend () {}	///< Called when plug-in is switched to off
    virtual void resume () {}	///< Called when plug-in is switched to on

    virtual void setSampleRate (float sampleRate);
    virtual void setBlockSize (VstInt32 blockSize);

    virtual void processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames) = 0;

private:
    PluginGUIConstructor *m_guiConstructor;
    QWidget *m_gui;
};
    

AudioVSTPluginIO::AudioVSTPluginIO(AudioCallbackRecordTarget *recordTarget,
				   AudioCallbackPlaySource *playSource,
                                   audioMasterCallback cb) :
    AudioCallbackIO(recordTarget, playSource),
    m_plugin(0)
{
    try {
	m_plugin = new Plugin(this, cb);
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

void
AudioVSTPluginIO::registerGui(QWidget *w)
{
    if (m_plugin) m_plugin->registerGui(w);
}

}

#endif
