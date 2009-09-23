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

class AudioVSTPluginIO : public AudioCallbackIO
{
public:
    AudioVSTPluginIO(AudioCallbackRecordTarget *recordTarget,
		     AudioCallbackPlaySource *playSource,
                     audioMasterCallback cb);
    virtual ~AudioVSTPluginIO();

    virtual bool isSourceOK() const;
    virtual bool isTargetOK() const;

    virtual double getCurrentTime() const;

    void registerGUIConstructor(PluginGUIConstructor *);

protected:
    class Plugin;
    Plugin *m_plugin;
};

}

#endif

#endif
