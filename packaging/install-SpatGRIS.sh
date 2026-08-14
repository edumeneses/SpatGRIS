#!/bin/sh
# Install this SpatGRIS bundle system-wide (or per-user with --user).
#
# SpatGRIS resolves Resources/ against the *current working directory*
# (Source/Misc/sg_DefaultFiles.hpp) and finds SpeakerView as a sibling of its own
# executable (Source/sg_MainComponent.cpp). So the payload has to stay together
# in one directory, and the launcher has to cd into it before exec'ing the
# binary. That is the entire reason this script exists rather than a plain
# `cp SpatGRIS /usr/local/bin`.

set -eu

SELF="$(readlink -f "$0")"
SRC="$(dirname "$SELF")"

APP=spatgris
MODE=install
USER_MODE=0
PREFIX=
BINDIR=
DATADIR=

usage() {
    cat <<EOF
Usage: $(basename "$0") [options]

  --user            Install for the current user only (no root needed).
  --prefix DIR      Where the payload goes.
                      system: /opt/spatgris        user: ~/.local/share/spatgris
  --bindir DIR      Where the launcher goes.
                      system: /usr/local/bin       user: ~/.local/bin
  --datadir DIR     Where .desktop and icons go.
                      system: /usr/share           user: ~/.local/share
  --uninstall       Remove a previous install (honours --user/--prefix).
  -h, --help        This message.

Run with no options for a system-wide install; the script re-runs itself under
sudo if it is not already root.
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --user)      USER_MODE=1 ;;
        --uninstall) MODE=uninstall ;;
        --prefix)    PREFIX="${2:?--prefix needs a directory}"; shift ;;
        --bindir)    BINDIR="${2:?--bindir needs a directory}"; shift ;;
        --datadir)   DATADIR="${2:?--datadir needs a directory}"; shift ;;
        -h|--help)   usage; exit 0 ;;
        *)           echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

# ── Defaults ──────────────────────────────────────────────────────────────────
if [ "$USER_MODE" -eq 1 ]; then
    : "${PREFIX:=${XDG_DATA_HOME:-$HOME/.local/share}/spatgris}"
    : "${BINDIR:=$HOME/.local/bin}"
    : "${DATADIR:=${XDG_DATA_HOME:-$HOME/.local/share}}"
else
    : "${PREFIX:=/opt/spatgris}"
    : "${BINDIR:=/usr/local/bin}"
    : "${DATADIR:=/usr/share}"

    # Re-exec under sudo for a system-wide install.
    if [ "$(id -u)" -ne 0 ]; then
        if command -v sudo >/dev/null 2>&1; then
            echo "Re-running under sudo for a system-wide install…"
            if [ "$MODE" = uninstall ]; then
                exec sudo -- "$SELF" --uninstall \
                     --prefix "$PREFIX" --bindir "$BINDIR" --datadir "$DATADIR"
            else
                exec sudo -- "$SELF" \
                     --prefix "$PREFIX" --bindir "$BINDIR" --datadir "$DATADIR"
            fi
        fi
        echo "Error: system-wide install needs root, and sudo is not available." >&2
        echo "Either run as root or use --user." >&2
        exit 1
    fi
fi

LAUNCHER="$BINDIR/$APP"
DESKTOP="$DATADIR/applications/$APP.desktop"
PIXMAP="$DATADIR/pixmaps/$APP.png"
ICON_SIZES="16 32 64 128 256 512"

refresh_caches() {
    # Both are best-effort: a missing tool is not an install failure.
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "$DATADIR/applications" 2>/dev/null || true
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -f -t "$DATADIR/icons/hicolor" 2>/dev/null || true
    fi
}

# ── Uninstall ─────────────────────────────────────────────────────────────────
if [ "$MODE" = uninstall ]; then
    echo "Removing SpatGRIS…"
    # Only delete the payload directory if it actually looks like our install.
    if [ -d "$PREFIX" ]; then
        if [ -x "$PREFIX/SpatGRIS" ]; then
            rm -rf "$PREFIX"
            echo "  removed $PREFIX"
        else
            echo "  skipped $PREFIX (no SpatGRIS binary there — not deleting it)" >&2
        fi
    fi
    rm -f "$LAUNCHER" "$DESKTOP" "$PIXMAP"
    # /etc/default/spatgris and ~/.config/spatgris.conf are left in place on
    # purpose: they hold the operator's port counts, not our shipped data.
    for s in $ICON_SIZES; do
        rm -f "$DATADIR/icons/hicolor/${s}x${s}/apps/$APP.png"
    done
    refresh_caches
    echo "Done."
    exit 0
fi

# ── Preflight ─────────────────────────────────────────────────────────────────
for f in SpatGRIS SpeakerView.x86_64 SpeakerView.pck; do
    [ -f "$SRC/$f" ] || { echo "Error: $f missing next to this script." >&2; exit 1; }
done
[ -d "$SRC/Resources" ] || { echo "Error: Resources/ missing next to this script." >&2; exit 1; }
[ -f "$SRC/spatgris-launcher.sh" ] || { echo "Error: spatgris-launcher.sh missing next to this script." >&2; exit 1; }

# Refuse to clobber a directory that is not a previous SpatGRIS install.
if [ -e "$PREFIX" ] && [ ! -x "$PREFIX/SpatGRIS" ]; then
    if [ -n "$(ls -A "$PREFIX" 2>/dev/null)" ]; then
        echo "Error: $PREFIX exists, is not empty, and holds no SpatGRIS binary." >&2
        echo "Refusing to overwrite it. Pass a different --prefix." >&2
        exit 1
    fi
fi

echo "Installing SpatGRIS"
echo "  payload  -> $PREFIX"
echo "  launcher -> $LAUNCHER"
echo "  desktop  -> $DESKTOP"

# ── Payload ───────────────────────────────────────────────────────────────────
rm -rf "$PREFIX"
mkdir -p "$PREFIX"
cp -a "$SRC/SpatGRIS" "$SRC/SpeakerView.x86_64" "$SRC/SpeakerView.pck" "$PREFIX/"
cp -a "$SRC/Resources" "$PREFIX/Resources"
chmod 0755 "$PREFIX/SpatGRIS" "$PREFIX/SpeakerView.x86_64"

# ── Launcher ──────────────────────────────────────────────────────────────────
mkdir -p "$BINDIR"
# One canonical launcher, shared with the .deb: cds into the payload, loads the
# JUCE_JACK_VIRTUAL_* config, and wraps in pw-jack when libjack.so.0 is not on
# the loader path.
sed "s|@PAYLOAD@|$PREFIX|" "$SRC/spatgris-launcher.sh" > "$LAUNCHER"
chmod 0755 "$LAUNCHER"

# Config holding the JACK port counts. Read by the launcher, so it also applies
# when SpatGRIS is started from the desktop menu with no shell profile loaded.
if [ "$USER_MODE" -eq 1 ]; then
    CONFIG="${XDG_CONFIG_HOME:-$HOME/.config}/spatgris.conf"
else
    CONFIG=/etc/default/spatgris
fi
if [ ! -e "$CONFIG" ]; then
    mkdir -p "$(dirname "$CONFIG")"
    cat > "$CONFIG" <<'EOF'
# SpatGRIS configuration.
#
# Number of ports for the JACK device named "Virtual ports (patch manually)".
# With either set, that device appears in SpatGRIS's audio settings; choosing it
# registers exactly this many ports, auto-connects nothing, and reports them all
# as active so you can patch the graph yourself (pw-link, qpwgraph, ...).
# Leave both unset to keep upstream behaviour, where the channel count comes from
# whichever other JACK client you select.
#
#JUCE_JACK_VIRTUAL_INPUTS=64
#JUCE_JACK_VIRTUAL_OUTPUTS=64

# Set to 1 to never wrap in pw-jack, even when libjack.so.0 is missing.
#SPATGRIS_NO_PW_JACK=1
EOF
    chmod 0644 "$CONFIG"
    echo "  config   -> $CONFIG (edit to set JACK port counts)"
fi

# ── Desktop entry ─────────────────────────────────────────────────────────────
mkdir -p "$DATADIR/applications"
if [ -f "$SRC/$APP.desktop" ]; then
    # Point Exec at the absolute launcher path: $BINDIR is not guaranteed to be
    # on PATH for the session that spawns desktop entries (notably ~/.local/bin).
    sed "s|^Exec=.*|Exec=$LAUNCHER|" "$SRC/$APP.desktop" > "$DESKTOP"
else
    cat > "$DESKTOP" <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=SpatGRIS
GenericName=Sound Spatialization
Comment=Sound spatialization tool — SAT dome build
Exec=$LAUNCHER
Icon=$APP
Terminal=false
Categories=AudioVideo;Audio;
StartupNotify=true
StartupWMClass=SpatGRIS
EOF
fi
chmod 0644 "$DESKTOP"

# ── Icons ─────────────────────────────────────────────────────────────────────
# Resources/AppIcon.iconset already ships exact square sizes, so no ImageMagick
# is needed. @2x files are the next size up: icon_128x128@2x.png is 256x256.
ICONSET="$SRC/Resources/AppIcon.iconset"
installed_icons=0
for s in $ICON_SIZES; do
    src=""
    if [ -f "$ICONSET/icon_${s}x${s}.png" ]; then
        src="$ICONSET/icon_${s}x${s}.png"
    else
        half=$((s / 2))
        if [ -f "$ICONSET/icon_${half}x${half}@2x.png" ]; then
            src="$ICONSET/icon_${half}x${half}@2x.png"
        fi
    fi
    [ -n "$src" ] || continue
    mkdir -p "$DATADIR/icons/hicolor/${s}x${s}/apps"
    cp -f "$src" "$DATADIR/icons/hicolor/${s}x${s}/apps/$APP.png"
    chmod 0644 "$DATADIR/icons/hicolor/${s}x${s}/apps/$APP.png"
    installed_icons=$((installed_icons + 1))
done

# Fallback for desktops that do not consult the hicolor theme.
mkdir -p "$DATADIR/pixmaps"
if [ -f "$SRC/$APP.png" ]; then
    cp -f "$SRC/$APP.png" "$PIXMAP"
elif [ -f "$ICONSET/icon_512x512.png" ]; then
    cp -f "$ICONSET/icon_512x512.png" "$PIXMAP"
fi
if [ -f "$PIXMAP" ]; then
    chmod 0644 "$PIXMAP"
fi

refresh_caches

if [ "$USER_MODE" -eq 1 ]; then
    UNINSTALL_HINT="$SELF --uninstall --user"
else
    UNINSTALL_HINT="$SELF --uninstall"
fi

echo
echo "Installed. $installed_icons icon size(s) registered."
echo "Launch with: $APP        (or find SpatGRIS in your application menu)"
case ":${PATH}:" in
    *":$BINDIR:"*) ;;
    *) echo "Note: $BINDIR is not on your PATH — add it, or run $LAUNCHER directly." ;;
esac
echo "Uninstall with: $UNINSTALL_HINT"
