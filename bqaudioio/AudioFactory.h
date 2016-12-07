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

    struct Preference {
        // In all cases an empty string indicates "choose
        // automatically". This may not be the same as any individual
        // preference, because it implies potentially trying more than
        // one implementation if the first doesn't work
        std::string implementation;
        std::string recordDevice;
        std::string playbackDevice;
        Preference() { }
    };
    
    static SystemRecordSource *createCallbackRecordSource(ApplicationRecordTarget *,
                                                          Preference);
    
    static SystemPlaybackTarget *createCallbackPlayTarget(ApplicationPlaybackSource *,
                                                          Preference);
    
    static SystemAudioIO *createCallbackIO(ApplicationRecordTarget *,
                                           ApplicationPlaybackSource *,
                                           Preference);
};

}

#endif

