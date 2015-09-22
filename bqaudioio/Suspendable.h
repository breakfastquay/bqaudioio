/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef BQAUDIOIO_SUSPENDABLE_H
#define BQAUDIOIO_SUSPENDABLE_H

namespace breakfastquay {

class Suspendable
{
public:
    virtual void suspend() = 0;
    virtual void resume() = 0;
};

}

#endif
