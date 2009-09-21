/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef _AUDIO_VST_PLUGIN_IO_H_
#define _AUDIO_VST_PLUGIN_IO_H_

#ifdef HAVE_VST

#include "AudioCallbackIO.h"

namespace Turbot {

class AudioVSTPluginIO : public AudioCallbackIO
{
public:
    AudioVSTPluginIO(AudioCallbackRecordTarget *recordTarget,
		     AudioCallbackPlaySource *playSource);
    virtual ~AudioVSTPluginIO();

    virtual bool isSourceOK() const;
    virtual bool isTargetOK() const;

    virtual double getCurrentTime() const;

protected:
    class Plugin;
    Plugin *m_plugin;
};

}

#endif

#endif
