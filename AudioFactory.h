/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifndef _AUDIO_FACTORY_H_
#define _AUDIO_FACTORY_H_

namespace Turbot {

class AudioCallbackRecordSource;
class AudioCallbackRecordTarget;
class AudioCallbackPlayTarget;
class AudioCallbackPlaySource;
class AudioCallbackIO;

class AudioFactory 
{
public:
    static AudioCallbackRecordSource *createCallbackRecordSource(AudioCallbackRecordTarget *);
    static AudioCallbackPlayTarget *createCallbackPlayTarget(AudioCallbackPlaySource *);
    static AudioCallbackIO *createCallbackIO(AudioCallbackRecordTarget *,
                                             AudioCallbackPlaySource *);
};

}

#endif

