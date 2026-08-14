# CLAUDE.md — SAT dome fork of SpatGRIS

Context for Claude Code (and any human) working in `edumeneses/SpatGRIS`.
Fork of `GRIS-UdeM/SpatGRIS`. Last updated **2026-08-14**.

## What this fork is for

Running SpatGRIS on the Satosphère dome machine at the SAT: Ubuntu 26.04,
PipeWire, an RME HDSPe MADI card fed from a Ferrofish A32pro Dante over MADI SFP.
Upstream is not wrong; it just assumes a workflow this room does not have.

Two behavioural changes, **both Linux-only**, plus Linux packaging. Everything
else tracks upstream and must stay mergeable with it.

## Repository conventions — read before editing

1. **Never commit changes into the JUCE submodule.** JUCE sits three levels down
   (`submodules/AlgoGRIS/submodules/StructGRIS/submodules/JUCE`) and is not ours.
   Behaviour changes go in `patches/`, applied by CI. Do not commit a submodule
   pointer bump.
2. **Avoid editing `SpatGRIS.jucer`.** CI rewrites what it needs on a throwaway
   checkout, keeping the file byte-identical to upstream so merges stay clean.
3. **Nothing Claude-related gets committed** except this file — `.gitignore`
   covers `.claude/`, `*.backupclaude*` and friends. If you cut a branch for
   upstream, leave `CLAUDE.md` out of it.
4. Patch order matters: `0001` then `0002`. `0002` is generated against the
   post-`0001` file.

## The two patches

### `patches/0001-juce-jack-no-autoconnect.patch`

JUCE's `JackAudioIODevice::open()` unconditionally connects the client's ports to
the selected peer. Now behind `JUCE_JACK_NO_AUTOCONNECT`, defined for the Linux
exporter only. Also renames the standalone JACK client `JUCEJack` → `SpatGRIS`
(`JUCE_JACK_CLIENT_NAME`'s `#ifndef` fallback, which only applies when
`JucePlugin_Name` is undefined — i.e. exactly the standalone-app case).

### `patches/0002-juce-jack-virtual-ports.patch`

Upstream derives a device's channel count from the port count of whichever
*other* JACK client you select (`JackAudioIODeviceType::scanForDevices` plus
`JackAudioIODevice::getChannelNames`), so "N ports connected to nothing" is not
expressible. Adds a synthetic device **"Virtual ports (patch manually)"**, shown
when `JUCE_JACK_VIRTUAL_INPUTS` / `JUCE_JACK_VIRTUAL_OUTPUTS` are set (read via
`SystemStats::getEnvironmentVariable`, capped at 512). It touches four places:

| location | change |
|---|---|
| `scanForDevices()` | advertises the device regardless of the rest of the graph |
| `get{Input,Output}ChannelNames()` | synthetic `in_1…in_N` instead of a peer's ports |
| `open()` | never auto-connects a virtual device |
| `updateActivePorts()` | reports virtual ports **active even when unconnected** |

That last row is the non-obvious one. Normally the active mask comes from
`jack_port_connected`, so an unwired device would report zero channels and the
count would shift as you patched — which would confuse the speaker setup.

Entire addition is inside `#if JUCE_LINUX || JUCE_BSD`. This matters:
`juce_JackAudio.cpp` compiles on macOS and Windows too, because `JUCE_JACK=1`
lives in `<JUCEOPTIONS>` (global, not per-exporter). On those platforms the
counts are hard-zero and behaviour is identical to upstream.

## Build facts that cost a CI round-trip each

- **SAF must be built before the app.** Upstream `main` links `saf`,
  `saf_example_binauraliser_nf`, `cblas`, `lapacke`, `fftw3f`, but the inherited
  `SpatGris-builds.yml` had no SAF step — which is why upstream `dev` fails. See
  README step 2 / `submodules/AlgoGRIS/build_scripts/`.
- **Must compile with Clang.** `SpatGRIS.jucer` sets `recommendedWarnings="LLVM"`,
  so Projucer emits Clang-only warning flags GCC rejects outright.
- **`-march=native` appears in four places**: twice in the `.jucer` (Debug and
  Release, both inside `LINUX_MAKE`) and twice in SAF's
  `framework/CMakeLists.txt:42-45`, where `SAF_ENABLE_SIMD` *force-writes* it into
  the CMake cache so `-D` overrides cannot win. All must be pinned for a
  redistributable (`x86-64-v3`), or an AVX-512 runner ships a binary that SIGILLs
  on the dome machine.
- **`.jucer` assumes the Fedora/Arch BLAS layout** (`/usr/include/openblas`,
  `-lcblas -llapacke`); on Ubuntu, bridge with symlinks.
- **SAF's SIMD is x86-only** (`saf_externals.h:293` hard-errors without
  SSE/SSE2/SSE3) yet the `.jucer` sets `SAF_ENABLE_SIMD=1` unconditionally, so
  upstream cannot build on aarch64 at all. On non-x86, strip the define and use
  `-mcpu=native` (Clang/AArch64 rejects `-march=native`).
- **`dunkelgrau/godot:4.7` no longer pulls.** `GRIS-UdeM/SpeakerView`'s own CI has
  been red on that `docker pull` since 2026-07-14. We install Godot from
  `godotengine/godot-builds` releases instead.

## Runtime facts that bite

- `Resources/` is resolved against the **current working directory**
  (`Source/Misc/sg_DefaultFiles.hpp`), while SpeakerView is found as a **sibling
  of the executable** (`Source/sg_MainComponent.cpp`). Hence the payload stays in
  one directory and every launcher `cd`s into it first. This is the whole reason
  `packaging/` exists rather than a plain `cp` onto `PATH`.
- **`pw-jack` is required on Debian/Ubuntu.** `pipewire-jack` installs its
  `libjack.so.0` into a private directory that is not on the loader path, so
  JUCE's `dlopen("libjack.so.0")` fails and the JACK backend silently never
  appears. Confirmed on the dome machine 2026-08-14. The launcher detects this and
  re-execs under `pw-jack`.
- Settings live in `~/.config/GRIS/SpatGRIS<version>.xml`, projects in
  `~/Documents`. Nothing user-owned is inside the payload, so reinstalling is
  safe — except custom templates dropped into `Resources/templates/`, which a
  reinstall wipes.
- The settings filename embeds the version, so bumping the version silently
  starts a fresh settings file (device selection resets). Upstream behaviour, but
  it looks like a packaging bug when it happens.

## PipeWire notes for the dome

- **No 64-channel limit in PipeWire.** `SPA_AUDIO_MAX_CHANNELS 64u` only sizes a
  fixed position array and is `#ifndef`-overridable; `spa_audio_info_raw.channels`
  is explicitly allowed to exceed it. The JACK-side ceiling is
  `jack.max-client-ports`, default **768** (`pipewire-jack.c:56`).
- **`jack.merge-monitor` defaults to true**, so a card's monitor ports share its
  client name and JUCE double-counts them — a 64-channel MADI card shows as **128
  inputs** (64 capture + 64 monitor). Set `jack.merge-monitor = false` per app via
  `PIPEWIRE_PROPS`, or in `~/.config/pipewire/jack.conf.d/`.
- Clock: the RME must be AutoSync/MADI, not Master, or its LED stays red. Pin
  PipeWire to the MADI rate with `default.clock.allowed-rates = [ <rate> ]` — a
  slaved card cannot be told a rate, so PipeWire must follow it.

## CI and releases

`.github/workflows/`

- **`linux-rolling-release.yml`** — SpeakerView (Godot) ‖ SpatGRIS (JUCE) →
  package. Produces the bundle zip **and** a `.deb`. Every push to `main`
  refreshes the `continuous` prerelease under stable asset names; `v*` tags cut a
  normal release.
- **`SpatGris-builds.yml`** — Linux-only compile test, matrix x86_64 + arm64.
  macOS/Windows jobs were removed: they need Apple signing secrets this fork does
  not have. `-march=native` is deliberately left alone here, since it is a
  compile test rather than a redistributable.

`packaging/`

- `spatgris-launcher.sh` — the single canonical launcher, `@PAYLOAD@` substituted
  at package time. The bundle's `run-SpatGRIS.sh`, the installer and the `.deb`
  all derive from it. Do not fork this logic.
- `install-SpatGRIS.sh` — portable installer (`--user`, `--uninstall`), for
  non-apt distros. Does **not** resolve dependencies; the launcher reports missing
  libraries with the apt command instead.
- `spatgris.desktop` — `Exec=` is rewritten to the absolute launcher path at
  install time, because neither `/usr/local/bin` nor `~/.local/bin` is reliably on
  PATH for whatever spawns desktop entries.

The `.deb` is the recommended install path: `Depends` is derived by
`dpkg-shlibdeps` from the ELF headers, plus the `dlopen`'d libraries it cannot
see (`pipewire-jack | libjack-jackd2-0 | libjack-0.125`, `libxcursor1`,
`libxinerama1`). Untagged builds version as `0.git<commit-date>.<sha>` — not
`0~git`, because GitHub rewrites `~` in stored asset names.
`/etc/default/spatgris` is a conffile, so operator port counts survive upgrades.

## Configuring the virtual JACK device

```sh
# /etc/default/spatgris  (deb / system install)
# ~/.config/spatgris.conf (--user install)
JUCE_JACK_VIRTUAL_INPUTS=64
JUCE_JACK_VIRTUAL_OUTPUTS=64
```

Then pick **Virtual ports (patch manually)** in SpatGRIS's audio settings and
wire the graph with `pw-link` / `qpwgraph`. Config is a file rather than an env
var so it also applies when launched from the desktop menu, where no shell
profile is read. Leave both unset for upstream behaviour.

## State as of 2026-08-14

- `main` = `3e88b76` — virtual ports + `.deb`, both workflows green.
- `continuous` release carries the zip and the `.deb`.
- Branches kept on purpose, not stale:
  - `ci/linux-jack-no-autoconnect` (`5d1c47a`) — for the upstream proposal.
  - `feat/jack-virtual-ports` (`f61db35`) — ditto.
  - `fix/deb-derive-depends` (`9f6b531`) — two CI fixes, awaiting merge.

**Verified:** builds green x86_64 + arm64; bundle layout matches the reference
zip; `Virtual ports (patch manually)` and both env var names present in the
shipped binary; installer tested end-to-end (6 icon sizes, `desktop-file-validate`
passes, clean uninstall, refuses to clobber a foreign `--prefix`); `.deb` control
valid and apt resolves its dependencies; no AVX-512 in the shipped binary.

**Not verified — needs a run on the dome:** that the virtual device actually
reports N in / N out with nothing patched. It is reasoned from the JUCE source and
compile-tested, not observed. Confirm that before treating it as working.

## Open items

1. Confirm the virtual device on the dome (above).
2. Upstream proposal, after that confirmation. **Include:** no-autoconnect, the
   `JUCEJack` → `SpatGRIS` rename (Edu's explicit call), the missing SAF build
   step, `CC/CXX=clang`, the BLAS layout fix, the aarch64 `SAF_ENABLE_SIMD` fix.
   **Exclude:** deleting upstream's macOS/Windows jobs, `SPEAKERVIEW_REPO`
   pointing at this fork, `-march=native` → `x86-64-v3`, the rolling-release
   workflow, and this file.
   A patch against a submodule is not upstreamable. Both behaviours can be had
   with **zero** JUCE modification, which is the version to propose: the rename via
   a `JUCE_JACK_CLIENT_NAME` define, and no-autoconnect via handing
   `AudioDeviceManager` empty channel masks (`open()` gates on
   `if (! inputChannels.isZero())`). The virtual device does need real JUCE
   changes, so propose it separately.
3. Undecided: whether to publish a stable-named `.deb` asset alongside the
   versioned one, for scripted downloads.
4. Consider having the installer carry over files added under
   `Resources/templates/` instead of wiping them on reinstall.
