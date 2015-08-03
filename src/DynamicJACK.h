/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef _DYNAMIC_JACK_H_
#define _DYNAMIC_JACK_H_

#ifdef HAVE_JACK

#include <jack/jack.h>
#include <dlfcn.h>
#include <QObject> // so as to pull in Q_OS_* etc

namespace Turbot {

//#define DEBUG_AUDIO_JACK_TARGET 1

#ifdef BUILD_STATIC
#ifdef Q_OS_LINUX

// Some lunacy to enable JACK support in static builds.  JACK isn't
// supposed to be linked statically, because it depends on a
// consistent shared memory layout between client library and daemon,
// so it's very fragile in the face of version mismatches.
//
// Therefore for static builds on Linux we avoid linking against JACK
// at all during the build, instead using dlopen and runtime symbol
// lookup to switch on JACK support at runtime.  The following big
// mess (down to the #endifs) is the code that implements this.

static void *symbol(const char *name)
{
    static bool attempted = false;
    static void *library = 0;
    static std::map<const char *, void *> symbols;
    if (symbols.find(name) != symbols.end()) return symbols[name];
    if (!library) {
        if (!attempted) {
            library = ::dlopen("libjack.so.1", RTLD_NOW);
            if (!library) library = ::dlopen("libjack.so.0", RTLD_NOW);
            if (!library) library = ::dlopen("libjack.so", RTLD_NOW);
            if (!library) {
                std::cerr << "WARNING: JACKPlaybackTarget: Failed to load JACK library: "
                          << ::dlerror() << " (tried .so, .so.0, .so.1)"
                          << std::endl;
            }
            attempted = true;
        }
        if (!library) return 0;
    }
    void *symbol = ::dlsym(library, name);
    if (!symbol) {
        std::cerr << "WARNING: JACKPlaybackTarget: Failed to locate symbol "
                  << name << ": " << ::dlerror() << std::endl;
    }
    symbols[name] = symbol;
    return symbol;
}

static int dynamic_jack_set_process_callback(jack_client_t *client,
                                             JackProcessCallback process_callback,
                                             void *arg)
{
    typedef int (*func)(jack_client_t *client,
                        JackProcessCallback process_callback,
                        void *arg);
    void *s = symbol("jack_set_process_callback");
    if (!s) return 1;
    func f = (func)s;
    return f(client, process_callback, arg);
}

static int dynamic_jack_set_xrun_callback(jack_client_t *client,
                                          JackXRunCallback xrun_callback,
                                          void *arg)
{
    typedef int (*func)(jack_client_t *client,
                        JackXRunCallback xrun_callback,
                        void *arg);
    void *s = symbol("jack_set_xrun_callback");
    if (!s) return 1;
    func f = (func)s;
    return f(client, xrun_callback, arg);
}

static const char **dynamic_jack_get_ports(jack_client_t *client, 
                                           const char *port_name_pattern, 
                                           const char *type_name_pattern, 
                                           unsigned long flags)
{
    typedef const char **(*func)(jack_client_t *client, 
                                 const char *port_name_pattern, 
                                 const char *type_name_pattern, 
                                 unsigned long flags);
    void *s = symbol("jack_get_ports");
    if (!s) return 0;
    func f = (func)s;
    return f(client, port_name_pattern, type_name_pattern, flags);
}

static jack_port_t *dynamic_jack_port_register(jack_client_t *client,
                                               const char *port_name,
                                               const char *port_type,
                                               unsigned long flags,
                                               unsigned long buffer_size)
{
    typedef jack_port_t *(*func)(jack_client_t *client,
                                 const char *port_name,
                                 const char *port_type,
                                 unsigned long flags,
                                 unsigned long buffer_size);
    void *s = symbol("jack_port_register");
    if (!s) return 0;
    func f = (func)s;
    return f(client, port_name, port_type, flags, buffer_size);
}

static int dynamic_jack_connect(jack_client_t *client,
                                const char *source,
                                const char *dest)
{
    typedef int (*func)(jack_client_t *client,
                        const char *source,
                        const char *dest);
    void *s = symbol("jack_connect");
    if (!s) return 1;
    func f = (func)s;
    return f(client, source, dest);
}

static void *dynamic_jack_port_get_buffer(jack_port_t *port,
                                          jack_nframes_t sz)
{
    typedef void *(*func)(jack_port_t *, jack_nframes_t);
    void *s = symbol("jack_port_get_buffer");
    if (!s) return 0;
    func f = (func)s;
    return f(port, sz);
}

static int dynamic_jack_port_unregister(jack_client_t *client,
                                        jack_port_t *port)
{
    typedef int(*func)(jack_client_t *, jack_port_t *);
    void *s = symbol("jack_port_unregister");
    if (!s) return 0;
    func f = (func)s;
    return f(client, port);
}

#define dynamic1(rv, name, argtype, failval) \
    static rv dynamic_##name(argtype arg) { \
        typedef rv (*func) (argtype); \
        void *s = symbol(#name); \
        if (!s) return failval; \
        func f = (func) s; \
        return f(arg); \
    }

dynamic1(jack_client_t *, jack_client_new, const char *, 0);
dynamic1(jack_nframes_t, jack_get_buffer_size, jack_client_t *, 0);
dynamic1(jack_nframes_t, jack_get_sample_rate, jack_client_t *, 0);
dynamic1(int, jack_activate, jack_client_t *, 1);
dynamic1(int, jack_deactivate, jack_client_t *, 1);
dynamic1(int, jack_client_close, jack_client_t *, 1);
dynamic1(jack_nframes_t, jack_port_get_latency, jack_port_t *, 0);
dynamic1(const char *, jack_port_name, const jack_port_t *, 0);

#define jack_client_new dynamic_jack_client_new
#define jack_get_buffer_size dynamic_jack_get_buffer_size
#define jack_get_sample_rate dynamic_jack_get_sample_rate
#define jack_set_process_callback dynamic_jack_set_process_callback
#define jack_set_xrun_callback dynamic_jack_set_xrun_callback
#define jack_activate dynamic_jack_activate
#define jack_deactivate dynamic_jack_deactivate
#define jack_client_close dynamic_jack_client_close
#define jack_get_ports dynamic_jack_get_ports
#define jack_port_register dynamic_jack_port_register
#define jack_port_unregister dynamic_jack_port_unregister
#define jack_port_get_latency dynamic_jack_port_get_latency
#define jack_port_name dynamic_jack_port_name
#define jack_connect dynamic_jack_connect
#define jack_port_get_buffer dynamic_jack_port_get_buffer

#endif
#endif

}

#endif

#endif
