/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/*
    bqaudioio

    Copyright 2007-2016 Particular Programs Ltd.

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Except as contained in this notice, the names of Chris Cannam and
    Particular Programs Ltd shall not be used in advertising or
    otherwise to promote the sale, use or other dealings in this
    Software without prior written authorization.
*/

#include "AudioFactory.h"

#include "JACKAudioIO.h"
#include "PortAudioIO.h"
#include "PulseAudioIO.h"

#include <iostream>

namespace breakfastquay {

SystemPlaybackTarget *
AudioFactory::createCallbackPlayTarget(ApplicationPlaybackSource *source)
{
    if (!source) {
        throw std::logic_error("ApplicationPlaybackSource must be provided");
    }

    SystemPlaybackTarget *target = 0;

#ifdef HAVE_JACK
    target = new JACKAudioIO(Mode::Playback, 0, source);
    if (target->isTargetOK()) return target;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackPlayTarget: Failed to open JACK target" << std::endl;
	delete target;
    }
#endif

#ifdef HAVE_LIBPULSE
    target = new PulseAudioIO(Mode::Playback, 0, source);
    if (target->isTargetOK()) return target;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackPlayTarget: Failed to open PulseAudio target" << std::endl;
	delete target;
    }
#endif

#ifdef HAVE_PORTAUDIO
    target = new PortAudioIO(Mode::Playback, 0, source);
    if (target->isTargetOK()) return target;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackPlayTarget: Failed to open PortAudio target" << std::endl;
	delete target;
    }
#endif

    std::cerr << "WARNING: AudioFactory::createCallbackPlayTarget: No suitable targets available" << std::endl;
    return 0;
}

SystemRecordSource *
AudioFactory::createCallbackRecordSource(ApplicationRecordTarget *target)
{
    if (!target) {
        throw std::logic_error("ApplicationRecordTarget must be provided");
    }

    SystemRecordSource *source = 0;

#ifdef HAVE_JACK
    source = new JACKAudioIO(Mode::Record, target, 0);
    if (source->isSourceOK()) return source;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackRecordSource: Failed to open JACK source" << std::endl;
	delete source;
    }
#endif

#ifdef HAVE_LIBPULSE
    source = new PulseAudioIO(Mode::Record, target, 0);
    if (source->isSourceOK()) return source;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackRecordSource: Failed to open PulseAudio source" << std::endl;
	delete source;
    }
#endif

#ifdef HAVE_PORTAUDIO
    source = new PortAudioIO(Mode::Record, target, 0);
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
    if (!target || !source) {
        throw std::logic_error("ApplicationRecordTarget and ApplicationPlaybackSource must both be provided");
    }

    SystemAudioIO *io = 0;

#ifdef HAVE_JACK
    io = new JACKAudioIO(Mode::Duplex, target, source);
    if (io->isOK()) return io;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackIO: Failed to open JACK I/O" << std::endl;
	delete io;
    }
#endif

#ifdef HAVE_LIBPULSE
    io = new PulseAudioIO(Mode::Duplex, target, source);
    if (io->isOK()) return io;
    else {
	std::cerr << "WARNING: AudioFactory::createCallbackIO: Failed to open PulseAudio I/O" << std::endl;
	delete io;
    }
#endif

#ifdef HAVE_PORTAUDIO
    io = new PortAudioIO(Mode::Duplex, target, source);
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
