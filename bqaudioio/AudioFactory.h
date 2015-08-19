/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef BQAUDIOIO_AUDIO_FACTORY_H
#define BQAUDIOIO_AUDIO_FACTORY_H

namespace breakfastquay {

class SystemRecordSource;
class ApplicationRecordTarget;
class SystemPlaybackTarget;
class ApplicationPlaybackSource;
class SystemAudioIO;

class AudioFactory 
{
public:
    static SystemRecordSource *createCallbackRecordSource(ApplicationRecordTarget *);
    static SystemPlaybackTarget *createCallbackPlayTarget(ApplicationPlaybackSource *);
    static SystemAudioIO *createCallbackIO(ApplicationRecordTarget *,
                                           ApplicationPlaybackSource *);
};

}

#endif

