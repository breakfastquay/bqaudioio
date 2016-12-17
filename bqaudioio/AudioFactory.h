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

#ifndef BQAUDIOIO_AUDIO_FACTORY_H
#define BQAUDIOIO_AUDIO_FACTORY_H

#include <vector>
#include <string>

namespace breakfastquay {

class SystemRecordSource;
class ApplicationRecordTarget;
class SystemPlaybackTarget;
class ApplicationPlaybackSource;
class SystemAudioIO;

class AudioFactory 
{
public:
    static std::vector<std::string> getImplementationNames();
    static std::string getImplementationDescription(std::string implName);
    static std::vector<std::string> getRecordDeviceNames(std::string implName);
    static std::vector<std::string> getPlaybackDeviceNames(std::string implName);

    /**
     * Preferences for implementation (i.e. audio driver layer) and
     * audio device.
     *
     * Wherever a non-empty string is provided, then it will be used
     * by the factory; if the factory can't open the requested driver,
     * or select a requested device, creation will fail.
     *
     * Wherever an empty string is provided, the driver will make an
     * automatic selection and may potentially try more than one
     * implementation or device if its first choice can't be used.
     */
    struct Preference {
        std::string implementation;
        std::string recordDevice;
        std::string playbackDevice;
        Preference() { }
    };

    /**
     * Open the audio driver for duplex (i.e recording + playback) I/O
     * using the given driver and device preferences. Provide the
     * given record target and play source objects to the audio I/O
     * and return the new audio I/O.
     *
     * Caller owns the returned object and must delete it when
     * done. Note that the record target and playback source must
     * outlive the returned IO object.
     *
     * Return a null pointer if the requested device could not be
     * opened, or, in the case where no preference was stated, if no
     * device could be opened.
     */
    static SystemAudioIO *
    createCallbackIO(ApplicationRecordTarget *,
                     ApplicationPlaybackSource *,
                     Preference);

    /**
     * Open the audio driver in record-only mode using the given
     * driver and device preferences. Provide the given record target
     * to the audio source and return the new audio source.
     *
     * Caller owns the returned object and must delete it when
     * done. Note that the record target must outlive the returned
     * source object.
     *
     * Return a null pointer if the requested device could not be
     * opened, or, in the case where no preference was stated, if no
     * device could be opened.
     */
    static SystemRecordSource *
    createCallbackRecordSource(ApplicationRecordTarget *,
                               Preference);
    
    /**
     * Open the audio driver in playback-only mode using the given
     * driver and device preferences. Provide the given playback
     * source to the audio source and return the new audio target.
     *
     * Caller owns the returned object and must delete it when
     * done. Note that the playback source must outlive the returned
     * target object.
     *
     * Return a null pointer if the requested device could not be
     * opened, or, in the case where no preference was stated, if no
     * device could be opened.
     */
    static SystemPlaybackTarget *
    createCallbackPlayTarget(ApplicationPlaybackSource *,
                             Preference);

private:
    AudioFactory(const AudioFactory &)=delete;
    AudioFactory &operator=(const AudioFactory &)=delete;
};

}

#endif

