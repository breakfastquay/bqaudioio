/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#include "AudioFactory.h"

#include "JACKPlaybackTarget.h"
#include "PulseAudioPlaybackTarget.h"
#include "PortAudioPlaybackTarget.h"

#include "JACKRecordSource.h"
#include "PortAudioRecordSource.h"

#include "JACKAudioIO.h"
#include "PortAudioIO.h"
#include "PulseAudioIO.h"

#include <iostream>

namespace breakfastquay {

SystemPlaybackTarget *
AudioFactory::createCallbackPlayTarget(ApplicationPlaybackSource *source)
{
    SystemPlaybackTarget *target = 0;

#ifdef HAVE_JACK
    target = new JACKPlaybackTarget(source);
    if (target->isTargetOK()) return target;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackTarget: Failed to open JACK target" << std::endl;
	delete target;
    }
#endif

#ifdef HAVE_LIBPULSE
    target = new PulseAudioPlaybackTarget(source);
    if (target->isTargetOK()) return target;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackTarget: Failed to open PulseAudio target" << std::endl;
	delete target;
    }
#endif

#ifdef HAVE_PORTAUDIO
    target = new PortAudioPlaybackTarget(source);
    if (target->isTargetOK()) return target;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackTarget: Failed to open PortAudio target" << std::endl;
	delete target;
    }
#endif

    std::cerr << "WARNING: AudioFactory::createCallbackTarget: No suitable targets available" << std::endl;
    return 0;
}

SystemRecordSource *
AudioFactory::createCallbackRecordSource(ApplicationRecordTarget *target)
{
    SystemRecordSource *source = 0;

#ifdef HAVE_JACK
    source = new JACKRecordSource(target);
    if (source->isSourceOK()) return source;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackRecordSource: Failed to open JACK source" << std::endl;
	delete source;
    }
#endif

#ifdef HAVE_PORTAUDIO
    source = new PortAudioRecordSource(target);
    if (source->isSourceOK()) return source;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackRecordSource: Failed to open PortAudio source" << std::endl;
	delete source;
    }
#endif

    std::cerr << "WARNING: AudioFactory::createCallbackRecordSource: No suitable sources available" << std::endl;
    return 0;
}

SystemAudioIO *
AudioFactory::createCallbackIO(ApplicationRecordTarget *target,
                               ApplicationPlaybackSource *source)
{
    SystemAudioIO *io = 0;

#ifdef HAVE_JACK
    io = new JACKAudioIO(target, source);
    if (io->isOK()) return io;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackIO: Failed to open JACK I/O" << std::endl;
	delete io;
    }
#endif

#ifdef HAVE_LIBPULSE
    io = new PulseAudioIO(target, source);
    if (io->isOK()) return io;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackIO: Failed to open PulseAudio I/O" << std::endl;
	delete io;
    }
#endif

#ifdef HAVE_PORTAUDIO
    io = new PortAudioIO(target, source);
    if (io->isOK()) return io;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackIO: Failed to open PortAudio I/O" << std::endl;
	delete io;
    }
#endif

    std::cerr << "WARNING: AudioFactory::createCallbackIO: No suitable I/Os available" << std::endl;
    return 0;
}

}
