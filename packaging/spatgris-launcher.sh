#!/bin/sh
# SpatGRIS launcher. @PAYLOAD@ is replaced at install/package time with the
# directory holding SpatGRIS, SpeakerView and Resources.
#
# Three jobs:
#  1. cd into the payload, because Resources/ is resolved against the working
#     directory (Source/Misc/sg_DefaultFiles.hpp).
#  2. load JUCE_JACK_VIRTUAL_* from config, so the JACK port counts survive
#     launches from a desktop menu where no shell profile is read.
#  3. wrap in pw-jack when libjack.so.0 is not on the loader path, which is the
#     case with Debian/Ubuntu's pipewire-jack: it installs its libjack into a
#     private directory, so SpatGRIS's dlopen("libjack.so.0") fails otherwise
#     and the JACK backend silently never appears.

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

JACK_WRAP=""
if [ -z "${SPATGRIS_NO_PW_JACK:-}" ]; then
    if ! ldconfig -p 2>/dev/null | grep -q 'libjack\.so\.0'; then
        if command -v pw-jack >/dev/null 2>&1; then
            JACK_WRAP="pw-jack"
            echo "spatgris: libjack.so.0 not on the loader path — running under pw-jack" >&2
        else
            echo "spatgris: warning: no libjack.so.0 and no pw-jack found; the JACK" >&2
            echo "spatgris:          backend will not appear. Install pipewire-jack." >&2
        fi
    fi
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
