
# Place in AUDIOIO_DEFINES the relevant options for the audio
# recording and playback APIs you wish to compile support for.
#
# Available options are
#
#  -DHAVE_PORTAUDIO     General cross-platform support using PortAudio
#  -DHAVE_JACK          Cross-platform low-latency audio using JACK
#  -DHAVE_LIBPULSE      Direct PulseAudio support on Linux
#
# You may define more than one of these.

AUDIOIO_DEFINES	:= -DHAVE_JACK -DHAVE_LIBPULSE -DHAVE_PORTAUDIO


# Add any related includes and libraries here
#
THIRD_PARTY_INCLUDES	:=
THIRD_PARTY_LIBS	:=


# If you are including a set of bq libraries into a project, you can
# override variables for all of them (including all of the above) in
# the following file, which all bq* Makefiles will include if found

-include ../Makefile.inc-bq


# This project-local Makefile describes the source files and contains
# no routinely user-modifiable parts

include build/Makefile.inc
