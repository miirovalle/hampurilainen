# LoopDeck

An **AUv3** (iOS) and **VST3** (Windows / macOS / Linux) **MIDI processor** that
plays every `.mid` loop in this repository and sends the notes to external synths
and drum machines — hardware or software.

Everything is one self contained plug-in: 2704 drum and percussion loops
(116 folders) are packed into the binary, so there are no files to import and
nothing to copy to the device.

```
Loops/*.mid  ──►  tools/build_loop_library.py  ──►  one packed blob
                                                        │
                                     CMake + juceaide ───┴──►  LoopDeck.appex (AUv3, aumi)
                                                               LoopDeck.vst3 (VST3 MIDI effect)
                                                               LoopDeck.app  (standalone host)
```

## What it does

* **4 slots**, each with its own loop, so you can layer a kick, a break and two
  percussion loops on separate MIDI channels.
* **Routing**: per slot MIDI channel, transpose (±48), drum note map, gate,
  velocity scale, fixed velocity, swing, start offset, speed (1/4×…4×),
  loop length override (1–16 bars) and direction (forward / reverse / ping-pong).
* **Clock**: host transport, internal tempo, or incoming **MIDI clock**.
* **MIDI thru** with optional channel force, and an input channel filter.
* **Pad triggering**: start slots from notes (C1 = slot 1 … D#1 = slot 4), so
  Drambo/AUM pads or a Launchpad can fire the loops.
* **Panic** button and automatic note-offs / all-notes-off on stop.

## Building

The plug-in is a CMake project (JUCE 8.0.15, fetched automatically).

### Desktop (Windows / macOS / Linux)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Standalone host, for fast iteration
cmake --build build --config Release --target LoopDeck_Standalone

# VST3 MIDI effect
cmake --build build --config Release --target LoopDeck_VST3
```

The VST3 bundle lands in
`build/LoopDeck_artefacts/Release/VST3/LoopDeck.vst3`. Copy it to
`C:\Program Files\Common Files\VST3` (Windows),
`~/Library/Audio/Plug-Ins/VST3` (macOS) or `~/.vst3` (Linux).

A VST3 MIDI effect has **no audio buses** (the same layout the AUv3 uses), so
the host has to support audio-less MIDI inserts. Reaper, Bitwig, Cubase and
Studio One do: insert LoopDeck before the instrument and route its MIDI output
onwards. Hosts that require an audio bus — Ableton Live, for instance — will not
load it.

### Engine tests

The MIDI engine has no GUI or host dependency, so it is tested headlessly
(including parsing all 2704 loops):

```sh
cmake -S . -B build
cmake --build build --config Release --target LoopDeckTests
./build/LoopDeckTests_artefacts/Release/LoopDeckTests
```

### iOS / AUv3

Needs macOS with Xcode. Targets **iPadOS 17.0 / iOS 17.0** and newer.

```sh
./scripts/build_ipa.sh              # -> dist/LoopDeck.ipa (unsigned)
```

or set the signing variables documented at the top of the script for a signed
IPA. `IOS_DEPLOYMENT_TARGET` raises or lowers the minimum OS version.

## Getting the VST3 from GitHub Actions

[`.github/workflows/build-vst3.yml`](.github/workflows/build-vst3.yml) builds the
VST3 on `windows-latest`, `macos-15` (universal arm64 + x86_64) and
`ubuntu-latest`, and uploads one zip per platform:

| Artifact | Contents |
| --- | --- |
| `LoopDeck-VST3-windows-x64-<sha>` | `LoopDeck.vst3` (`Contents/x86_64-win`) |
| `LoopDeck-VST3-macos-universal-<sha>` | `LoopDeck.vst3` (universal) |
| `LoopDeck-VST3-linux-x64-<sha>` | `LoopDeck.vst3` (`Contents/x86_64-linux`) |

It runs on a push to `main`, on a `v*` tag, or from
**Actions → Build VST3 → Run workflow**. The job fails if the bundle binary or
its `Contents/Resources/moduleinfo.json` manifest is missing.

## Getting the IPA from GitHub Actions

[`.github/workflows/build-auv3-ipa.yml`](.github/workflows/build-auv3-ipa.yml)
builds the IPA on `macos-15` and uploads it as the `LoopDeck-AUv3-<sha>`
artifact. It runs on a push to `main`, on a `v*` tag, or from
**Actions → Build AUv3 IPA → Run workflow**.

Out of the box the IPA is **unsigned**. That is what AltStore, Sideloadly and
TrollStore expect: they sign it during installation with your own Apple ID.

For a signed IPA, add these repository secrets:

| Secret | Value |
| --- | --- |
| `IOS_CERTIFICATE_BASE64` | `base64 -i cert.p12 \| pbcopy` |
| `IOS_CERTIFICATE_PASSWORD` | password of the `.p12` |
| `IOS_TEAM_ID` | your 10 character team id |
| `IOS_SIGNING_IDENTITY` | e.g. `Apple Distribution: Name (TEAMID)` |
| `IOS_PROVISIONING_PROFILE_BASE64` | optional `base64 -i profile.mobileprovision` |
| `IOS_PROVISIONING_PROFILE_SPECIFIER` | optional profile name |
| `IOS_EXPORT_METHOD` | optional: `development`, `ad-hoc`, `app-store-connect` |

A wildcard App ID for `com.miiroaudio.loopdeck.*` covers both the app and its
extension (the appex is `com.miiroaudio.loopdeck.loopdeckAUv3`).

## Installing on the iPad / iPhone

Requires **iPadOS 17.0 / iOS 17.0** or newer.

1. Download the IPA artifact.
2. Install with AltStore, Sideloadly, or Xcode (`Window → Devices and Simulators`).
3. Launch LoopDeck once — iOS only registers the audio unit after the host app
   has run.

## Using it in AUM / Koala / Drambo

LoopDeck appears as a **MIDI processor** (`aumi`), listed as
**Miiro Audio: LoopDeck**.

* Insert it in a **MIDI effect slot** on the track whose instrument (your synth
  or drum machine) should receive the notes.
* Its MIDI **output** carries the loops; connect that to the destination.
* Loading an AUv3 MIDI effect directly in front of an instrument also works in
  hosts that chain MIDI effects into the instrument node (Koala, Drambo).

Set each slot's **output channel** to match what the receiving device listens on
(10 for General MIDI drums), and pick the **note map** that matches the target:
`GM Standard`, `TR-808 style`, `TR-909 style`, `MPC style`, or `Custom` — the
Custom map is edited note by note in the **NOTE MAP** tab, so any drum machine's
layout can be matched exactly.

## Layout

```
Source/
  engine/     MidiEngine (timing, warping, routing), LoopLibrary (packed blob), DrumMap
  model/      parameters
  ui/         theme, widgets, slot rows, browser, routing / note map panels
  PluginProcessor.*  PluginEditor.*
tools/        build_loop_library.py, make_icon.py
Tests/        headless engine tests
scripts/      build_ipa.sh
Resources/    AppIcon.png (single 1024pt source for every icon size)
.github/      workflows: build-vst3.yml, build-auv3-ipa.yml
```
