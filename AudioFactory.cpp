/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#include "AudioFactory.h"

#include "AudioJACKTarget.h"
#include "AudioPortAudioTarget.h"

#include "AudioJACKSource.h"
#include "AudioPortAudioSource.h"

#include "AudioJACKIO.h"
#include "AudioPortAudioIO.h"
#include "AudioPulseAudioIO.h"

#include <iostream>

namespace Turbot {

AudioCallbackPlayTarget *
AudioFactory::createCallbackPlayTarget(AudioCallbackPlaySource *source)
{
    AudioCallbackPlayTarget *target = 0;

#ifdef HAVE_JACK
    target = new AudioJACKTarget(source);
    if (target->isTargetOK()) return target;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackTarget: Failed to open JACK target" << std::endl;
	delete target;
    }
#endif

#ifdef HAVE_PORTAUDIO
    target = new AudioPortAudioTarget(source);
    if (target->isTargetOK()) return target;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackTarget: Failed to open PortAudio target" << std::endl;
	delete target;
    }
#endif

    std::cerr << "WARNING: AudioFactory::createCallbackTarget: No suitable targets available" << std::endl;
    return 0;
}

AudioCallbackRecordSource *
AudioFactory::createCallbackRecordSource(AudioCallbackRecordTarget *target)
{
    AudioCallbackRecordSource *source = 0;

#ifdef HAVE_JACK
    source = new AudioJACKSource(target);
    if (source->isSourceOK()) return source;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackRecordSource: Failed to open JACK source" << std::endl;
	delete source;
    }
#endif

#ifdef HAVE_PORTAUDIO
    source = new AudioPortAudioSource(target);
    if (source->isSourceOK()) return source;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackRecordSource: Failed to open PortAudio source" << std::endl;
	delete source;
    }
#endif

    std::cerr << "WARNING: AudioFactory::createCallbackRecordSource: No suitable sources available" << std::endl;
    return 0;
}

AudioCallbackIO *
AudioFactory::createCallbackIO(AudioCallbackRecordTarget *target,
                               AudioCallbackPlaySource *source)
{
    AudioCallbackIO *io = 0;

#ifdef HAVE_JACK
    io = new AudioJACKIO(target, source);
    if (io->isSourceOK() && io->isTargetOK()) return io;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackIO: Failed to open JACK I/O" << std::endl;
	delete io;
    }
#endif

#ifdef HAVE_LIBPULSE
    io = new AudioPulseAudioIO(target, source);
    if (io->isSourceOK() && io->isTargetOK()) return io;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackIO: Failed to open PulseAudio I/O" << std::endl;
	delete io;
    }
#endif

#ifdef HAVE_PORTAUDIO
    io = new AudioPortAudioIO(target, source);
    if (io->isSourceOK() && io->isTargetOK()) return io;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackIO: Failed to open PortAudio I/O" << std::endl;
	delete io;
    }
#endif

    std::cerr << "WARNING: AudioFactory::createCallbackIO: No suitable I/Os available" << std::endl;
    return 0;
}

}
