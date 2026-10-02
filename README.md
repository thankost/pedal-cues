# PedalCues

Drag-and-drop MIDI cues for the **Neural DSP Quad Cortex / QC Mini** and the **DigiTech Whammy V**.
A VST3 / AU / LV2 / Standalone plugin (JUCE) for Windows, macOS and Linux. Tested in Reaper; it should work in any DAW that accepts dragged MIDI files and can send a MIDI track to a hardware output (Ableton Live, Cubase, Bitwig, Studio One; in Logic the clips work on an External MIDI track).

**[Download for Windows and macOS](https://thankost.github.io/pedal-cues/)**

Drag a tile onto the arrangement → a **named MIDI item** lands at the drop position (e.g. `QC Scene B - Chorus`,
`Whammy Oct Up [Chords]`, `Whammy Ramp Up 1 bar`). Click a tile's ▶ corner to send it to the pedal right away.

![PedalCues - Quad Cortex page](docs/images/quad-cortex.png)

📖 **[User guide with pictures and step-by-step instructions](https://thankost.github.io/pedal-cues/guide.html)** · 📝 **[What's new](https://thankost.github.io/pedal-cues/changelog.html)** · 🛟 **[Help and problem reports](https://thankost.github.io/pedal-cues/help.html)**. A quick tour also starts the first time you open the plugin (open it again later from the **☰** menu).

| Whammy V | MIDI Setup |
|---|---|
| ![Whammy page](docs/images/whammy.png) | ![MIDI Setup tab](docs/images/settings.png) |

## Features

**Quad Cortex**
- Preset tiles (name, colour, setlist / bank / slot) → `CC#0` page [+ `CC#32` setlist] + Program Change
- **Sync from QC (USB):** reads your setlists, preset names (bank/slot), and scene names, scene colours and stomp names straight from the pedal (quit Cortex Control first). Optional "every preset" mode loads each preset in turn to read it
- 8 scene tiles per preset in gig-view colours → `CC#43`
- Scene and stomp tiles either **load their own preset first** (default; the scene/footswitch follows 1/16 later)
  or act on the **current QC preset** only
- Footswitch stomps A–H on/off → `CC#35–42`; tuner on/off → `CC#45`; gig mode → `CC#47`

**Whammy V**
- All 21 modes, Classic or Chords, engaged or bypassed (Program Change)
- Optional "heel before mode change" (`CC#11 = 0`)
- Treadle moves on `CC#11`: Ramp Up, Ramp Down, Rise & Fall, Dive, Trill (1/16), Bend to Bar, Toe, Heel
- Draw your own treadle move with the mouse, then drag it onto the timeline like any other move
- Moves are written in beats (1/16 to 4 bars), so they follow the project tempo; adjustable curve;
  optional return to heel afterwards

**Workflow**
- Update notice: the header shows your version and whether a newer release is out (one GitHub request when it opens; turn it off in the ☰ menu)
- Everything is stored in your DAW project. **☰ > Save as default setup** makes new instances start with your names, colours and MIDI settings
- **☰ > Export / Import setup** as a file (back it up, share it with the band)
- MIDI passes through, so the dropped items and the live preview share one track and one route

Preset/scene names are entered in the plugin (double-click to rename, right-click for colour/reorder).
CueDrop-style USB sync would rely on Neural DSP's undocumented protocol, so it isn't included.

## Install (no code needed)

Download the zip for your system from the [website](https://thankost.github.io/pedal-cues/) or the [Releases page](https://github.com/thankost/pedal-cues/releases/latest).

**Windows**
1. Unzip, copy `PedalCues.vst3` to `C:\Program Files\Common Files\VST3\`.
2. Re-scan plug-ins in your DAW (Reaper: *Options > Preferences > Plug-ins > VST > Re-scan*). `PedalCues.exe` is the standalone version.

**macOS** (Apple Silicon and Intel, macOS 11 or later)
1. Unzip. The build is not notarised by Apple, so clear the download quarantine once in Terminal:
   `xattr -cr ~/Downloads/PedalCues-macOS`
   (without this, macOS says *"PedalCues is damaged and can't be opened"*).
2. Drag `PedalCues.app` (standalone) to *Applications*. Copy `PedalCues.vst3` to `~/Library/Audio/Plug-Ins/VST3/` and
   `PedalCues.component` to `~/Library/Audio/Plug-Ins/Components/`.
3. Re-scan plug-ins in your DAW.

**Linux** (new; x86-64, Ubuntu 22.04+ / Debian 12 / Fedora and similar)
1. Unzip `PedalCues-Linux.zip`. Copy `PedalCues.vst3` to `~/.vst3/` and `PedalCues.lv2` to `~/.lv2/`; run `./PedalCues` for the standalone app.
2. For *Sync from QC* over USB, add the one-time permission rule:
   `echo 'KERNEL=="hidraw*", ATTRS{idVendor}=="152a", TAG+="uaccess"' | sudo tee /etc/udev/rules.d/70-quad-cortex.rules && sudo udevadm control --reload-rules && sudo udevadm trigger`
   then replug the QC. Details: [guide, Install](https://thankost.github.io/pedal-cues/guide.html#1-install).

To publish a new release: `git tag v0.1.1 && git push origin v0.1.1`.

## Build (macOS)

```bash
brew install cmake ninja
bash build.sh
```

The build installs the VST3 and AU plugins to `~/Library/Audio/Plug-Ins` and also produces a standalone app.
If `external/JUCE` is missing, the script clones JUCE 8.0.8 there.

Run the MIDI logic tests:

```bash
cmake --build build --target PedalCuesTests
./build/PedalCuesTests_artefacts/Release/PedalCuesTests
```

## DAW setup (Reaper as the example)

1. Connect the pedals one of three ways (the plugin's **MIDI Setup** tab shows the steps):
   - **Daisy chain:** interface **MIDI Out → QC MIDI In**, QC **MIDI Thru → Whammy MIDI In** (MIDI Thru on). Both cue tracks output to that interface MIDI Out.
   - **Separate MIDI cables:** interface **MIDI Out 1 → QC**, **MIDI Out 2 → Whammy**. QC Cues → MIDI Out 1, Whammy Cues → MIDI Out 2.
   - **QC over USB + interface:** QC on USB, interface **MIDI Out → Whammy MIDI In**. QC Cues → Quad Cortex, Whammy Cues → interface MIDI Out.
   - ⚠️ **Known QC limitation:** MIDI Thru doesn't forward MIDI the QC receives over **USB**, so "QC on USB, Whammy on the QC's Thru" doesn't work.
2. Give the pedals **different MIDI channels** (defaults: QC 1 via *Settings > MIDI Settings*, not Omni; Whammy 2, see its manual) and set the same numbers in the plugin's **MIDI Setup** tab.
3. In your DAW, enable the MIDI outputs you use (Reaper: *Preferences > MIDI Devices*).
4. Create two tracks, **QC Cues** and **Whammy Cues**, insert *PedalCues* on each, and set each track's MIDI output as above (Reaper: *I/O > MIDI Hardware Output*, leave *Send to original channels*).
5. Turn snapping on and drag QC tiles onto QC Cues and Whammy tiles onto Whammy Cues at the bars you want.

Details and pictures: [guide, section 2](https://thankost.github.io/pedal-cues/guide.html#2-connect-your-rig).

## Verify with your pedals

- **Whammy numbering:** the plugin uses the manual's 1-based numbers (Classic 1–21 on / 22–42 bypassed,
  Chords 43–63 on / 64–84 bypassed). If every mode arrives one step off, change *MIDI Setup → Whammy program
  numbering*. Mode names can be renamed if your chart differs.
- **QC setlist:** `CC#32` = setlist number − 1. Leave *Send setlist* off if all presets are in the active setlist.

## Layout

```
Source/CueModel.*        MIDI definitions for both pedals + .mid file writer
Source/State.*           ValueTree schema, defaults, setup (names + MIDI settings) save/load
Source/Tile.*            draggable tile (external file drag + click-to-send)
Source/PluginProcessor.* MIDI passthrough, preview scheduling, host tempo, state
Source/Theme.*           colour palette + custom LookAndFeel
Source/PluginEditor.*    window, header tabs, first-run tour host
Source/QcPage.cpp        Quad Cortex page    Source/WhammyPage.cpp  Whammy V page
Source/SettingsPage.cpp  MIDI Setup tab (your pedals, DAW tracks, test) and the wiring guide
Source/Tour.*            quick-tour overlay (steps + spotlight)
Tools/DocShots.cpp       renders docs/images/*.png
docs/GUIDE.md            user guide
```

## Regenerating the documentation images

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPEDALCUES_BUILD_DOCS=ON
cmake --build build --target DocShots
build/DocShots_artefacts/Release/DocShots docs/images
```

## Support

PedalCues is free. If it helps your show and you'd like to say thanks, you can donate (completely optional):
[Buy Me a Coffee](https://buymeacoffee.com/athkost) · [PayPal](https://paypal.me/athkost) · [Revolut](https://revolut.me/athkost). In the plugin: **☰ > Support PedalCues**.

## License

Free and open source under the [MIT License](LICENSE). Copyright (c) 2026 **Thanasis Kostopoulos**. [github.com/thankost/pedal-cues](https://github.com/thankost/pedal-cues)

Built with [JUCE](https://juce.com), which is licensed separately (AGPLv3 / JUCE licence), and [hidapi](https://github.com/libusb/hidapi) (BSD licence option). The USB sync follows the protocol documented by [pyquadcortex](https://github.com/stokes-audio/pyquadcortex) (MIT). Quad Cortex is a trademark of Neural DSP Technologies and Whammy is a trademark of DigiTech. This project is not affiliated with either company.
