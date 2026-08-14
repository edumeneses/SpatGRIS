#!/bin/sh
# SpatGRIS launcher. @PAYLOAD@ is replaced at install/package time with the
# directory holding SpatGRIS, SpeakerView and Resources.
#
# Three jobs:
#  1. cd into the payload, because Resources/ is resolved against the working
#     directory (Source/Misc/sg_DefaultFiles.hpp).
#  2. load JUCE_JACK_VIRTUAL_* from config, so the JACK port counts survive
#     launches from a desktop menu where no shell profile is read.
#  3. wrap in pw-jack when PipeWire is the live JACK server. Debian/Ubuntu's
#     pipewire-jack installs its libjack into a private directory, so
#     SpatGRIS's dlopen("libjack.so.0") does not find it; pw-jack puts that
#     directory on LD_LIBRARY_PATH.

set -eu

PAYLOAD="@PAYLOAD@"

# Later files win, so a user config overrides the system default.
for cfg in /etc/default/spatgris "${XDG_CONFIG_HOME:-$HOME/.config}/spatgris.conf"; do
    if [ -r "$cfg" ]; then
        # shellcheck disable=SC1090
        . "$cfg"
    fi
done

# Exporting an unset name is harmless: the variable stays unset and JUCE's
# getEnvironmentVariable returns empty, which means "no virtual device".
export JUCE_JACK_VIRTUAL_INPUTS JUCE_JACK_VIRTUAL_OUTPUTS 2>/dev/null || true

# Which libjack to use is decided by which server is live, not by whether some
# libjack.so.0 exists. Installing jackd2 — qjackctl, jacktrip and jackd2-firewire
# all pull in libjack-jackd2-0 — puts jackd2's client library on the loader path
# even when jackd is not running and PipeWire owns the card. Testing only for the
# file's presence then skips the wrap, and SpatGRIS dlopen()s a client library
# that tries to reach a server that is not there.
if [ -n "${SPATGRIS_PW_JACK:-}" ]; then
    want_pw_jack=1                  # operator override: always wrap
elif [ -n "${SPATGRIS_NO_PW_JACK:-}" ]; then
    want_pw_jack=0                  # operator override: never wrap
elif pgrep -x jackd >/dev/null 2>&1; then
    want_pw_jack=0                  # a real JACK server holds the card
elif [ -S "${PIPEWIRE_RUNTIME_DIR:-${XDG_RUNTIME_DIR:-/run/user/$(id -u)}}/${PIPEWIRE_REMOTE:-pipewire-0}" ]; then
    want_pw_jack=1                  # PipeWire session is up
else
    want_pw_jack=0
fi

JACK_WRAP=""
if [ "$want_pw_jack" -eq 1 ]; then
    if command -v pw-jack >/dev/null 2>&1; then
        JACK_WRAP="pw-jack"
    else
        echo "spatgris: warning: PipeWire is running but pw-jack was not found, so" >&2
        echo "spatgris:          the JACK backend will be missing or will talk to the" >&2
        echo "spatgris:          wrong server. Install pipewire-jack." >&2
    fi
elif ! ldconfig -p 2>/dev/null | grep -q 'libjack\.so\.0'; then
    echo "spatgris: warning: no libjack.so.0 and no PipeWire session; the JACK" >&2
    echo "spatgris:          backend will not appear. Install pipewire-jack." >&2
fi

cd "$PAYLOAD" || exit 1

# Report missing shared libraries usefully instead of letting ld.so fail with a
# bare "cannot open shared object file".
if command -v ldd >/dev/null 2>&1; then
    missing="$(ldd ./SpatGRIS 2>/dev/null | awk '/not found/ {print $1}' | sort -u)"
    if [ -n "$missing" ]; then
        echo "spatgris: missing shared libraries:" >&2
        echo "$missing" | sed 's/^/  /' >&2
        echo "spatgris: on Debian/Ubuntu, install the runtime packages:" >&2
        echo "spatgris:   sudo apt install libopenblas0 liblapacke libfftw3-single3" >&2
        exit 1
    fi
fi

exec ${JACK_WRAP:+$JACK_WRAP} ./SpatGRIS "$@"
