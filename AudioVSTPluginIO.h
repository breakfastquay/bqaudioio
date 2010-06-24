/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef _AUDIO_VST_PLUGIN_IO_H_
#define _AUDIO_VST_PLUGIN_IO_H_

#ifdef HAVE_VST

#include <vst2.x/audioeffect.h>

#include "AudioCallbackIO.h"

class QWidget;

namespace Turbot {

class PluginGUIConstructor
{
public:
    virtual QWidget *construct() = 0;
};

class AudioVSTPluginIO;

class AudioVSTPlugin : public AudioEffect
{
public:
    AudioVSTPlugin(audioMasterCallback cb);
    virtual ~AudioVSTPlugin() { }

    void AudioVSTPluginIO *getIO();

    void registerGUIConstructor(PluginGUIConstructor *p); //???

    // Do not implement getEffectName -- leave to subclass

    // Do not implement getProductString -- leave to subclass

    virtual void getVendorString(char *n);

    virtual void open () {}	///< Called when plug-in is initialized
    virtual void close () {}	///< Called when plug-in will be released
    virtual void suspend () {}	///< Called when plug-in is switched to off
    virtual void resume () {}	///< Called when plug-in is switched to on

    virtual void setSampleRate (float sampleRate);
    virtual void setBlockSize (VstInt32 blockSize);

    float getSampleRate() { return m_sampleRate; }
    int getBlockSize() { return m_blockSize; }

    bool isOK() {
        return m_sampleRate > 0 && m_blockSize > 0;
    }

    // Do not implement processReplacing -- leave to subclass

private:
    float m_sampleRate;
    int m_blockSize;
    AudioVSTPluginIO *m_io;
    PluginGUIConstructor *m_guiConstructor;
    QWidget *m_gui;
};

class AudioVSTPluginIO : public AudioCallbackIO
{
public:
    AudioVSTPluginIO(AudioCallbackRecordTarget *recordTarget,
		     AudioCallbackPlaySource *playSource,
                     AudioVSTPlugin *plugin);
    virtual ~AudioVSTPluginIO();

    virtual bool isSourceOK() const;
    virtual bool isTargetOK() const;

    virtual double getCurrentTime() const;

    void registerGUIConstructor(PluginGUIConstructor *);//???

protected:
    AudioVSTPlugin *m_plugin;
};

}

#endif

#endif
