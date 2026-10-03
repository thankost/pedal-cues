# PedalCues

Drag-and-drop MIDI cues for the **Neural DSP Quad Cortex / QC Mini**, the **Kemper Profiler / Kemper Player** and the **DigiTech Whammy V / Whammy DT**, plus **any other MIDI device** with tiles you make yourself (beta).
A VST3 / AU / LV2 / Standalone plugin (JUCE) for Windows, macOS and Linux. Tested in Reaper; it should work in any DAW that accepts dragged MIDI files and can send a MIDI track to a hardware output (Ableton Live, Cubase, Bitwig, Studio One; in Logic the clips work on an External MIDI track).

**[Download for Windows and macOS](https://thankost.github.io/pedal-cues/)**

Drag a tile onto the arrangement → a **named MIDI item** lands at the drop position (e.g. `QC Scene B - Chorus`,
`Whammy Oct Up [Chords]`, `Whammy Ramp Up 1 bar`). Click a tile's ▶ corner to send it to the pedal right away.

![PedalCues - Quad Cortex page](docs/images/quad-cortex.png)

📖 **[User guide with pictures and step-by-step instructions](https://thankost.github.io/pedal-cues/guide.html)** · 📝 **[What's new](https://thankost.github.io/pedal-cues/changelog.html)** · 🛟 **[Help and problem reports](https://thankost.github.io/pedal-cues/help.html)**. A quick tour also starts the first time you open the plugin (open it again later from the **☰** menu).

| Kemper | Whammy V / DT | MIDI Setup |
|---|---|---|
| ![Kemper page](docs/images/kemper.png) | ![Whammy page](docs/images/whammy.png) | ![MIDI Setup tab](docs/images/settings.png) |

## Features

**Quad Cortex**
- Preset tiles (name, colour, setlist / bank / slot) → `CC#0` page [+ `CC#32` setlist] + Program Change
- **Fuzzy search** in the preset list (also Kemper performances and custom device tiles): letters in order, one typo, or a location like `SL2` / `3B`
- **Sync from QC (USB):** reads your setlists, preset names (bank/slot), and scene names, scene colours and stomp names straight from the pedal (quit Cortex Control first). Optional "every preset" mode loads each preset in turn to read it. The QC doesn't report setlist numbers, so you check them once in the sync window and PedalCues remembers them; the sync also turns on *Send setlist*
- 8 scene tiles per preset in gig-view colours → `CC#43`
- Scene and stomp tiles either **load their own preset first** (default; the scene/footswitch follows 1/16 later)
  or act on the **current QC preset** only
- Footswitch stomps A–H on/off → `CC#35–42`; tuner on/off → `CC#45`; Gig View screen on/off → `CC#46`; footswitch mode (preset / stomp / scene) → `CC#47`
- **Expression automation** (Quad Cortex page > **Expression**) → `CC#1` / `CC#2`: moves whatever you assign to
  Expression 1 or 2 on the QC (volume swells, wah, a delay mix, drive). Swell In, Fade Out, Rise & Fall, Slow Rise,
  Wah Rhythm, Rise to Bar, Toe, Heel, fixed positions (heel / 25% / half / 75% / toe), or draw your own; tempo-synced.
  Acts on the loaded preset, or optionally **loads the open preset first** (safe if a preset gets changed by accident)

**Kemper Profiler / Kemper Player** (pick the unit with the ▾ on the first tab, or in MIDI Setup)
- Performance tiles with five slots each → bank select `CC#32` + Program Change (Player: Program Change, 10 banks of 5)
- Slot tiles either **load their performance first** (default) or switch a slot of the **current performance** → `CC#50–54`
- Effect modules A, B, C, D, X, MOD, DLY, REV on/off (`CC#17–29`, delay and reverb with or without tails)
- Tuner (`CC#31`), Tap x4 (`CC#30`), Morph (`CC#80`), Rotary fast/slow (`CC#33`)
- **Pedal moves** on Wah (`CC#1`), Pitch (`CC#4`), Volume (`CC#7`) or Morph (`CC#11`): the same shapes, Set to tiles and Draw as QC expression
- Built from Kemper's MIDI documentation; not tested on a real Kemper yet

**Custom MIDI devices (beta)**: any device that takes MIDI (Fractal, Helix, Boss, a synth, a looper...)
- Groups and tiles you name yourself; each tile is one or more standard messages (Program Change, Control Change, bank select), set up in a guided editor with ready-made starting points and a Test button
- Programs counted from 0 or 1, as the device's manual does; notes on the device and on each tile
- **Export / Import device** as a `.pedalcues-device` file, so one person sets up a device and everyone with the same gear imports it

**Whammy V / Whammy DT** (pick the model with the switch on the Whammy page)
- All 21 modes, Classic or Chords (Chords on the V only), engaged or bypassed (Program Change)
- **Whammy DT Drop Tune:** Shift Up and Shift Down tiles, 1–7 semitones, Oct and Oct + Dry, on or bypassed (Program Change 43–78)
- Optional "heel before mode change" (`CC#11 = 0`)
- Treadle moves on `CC#11`: Ramp Up, Ramp Down, Rise & Fall, Dive, Trill (1/16), Bend to Bar, Toe, Heel
- Draw your own treadle move with the mouse, then drag it onto the timeline like any other move
- **My drawings:** save your drawn moves by name, load them to reuse or edit, rename or delete them. Kept on your computer
  (every project sees them, updates keep them), shared by the Whammy treadle and QC expression, and included in Export setup
- Moves are written in beats (1/16 to 4 bars), so they follow the project tempo; adjustable curve;
  optional return to heel afterwards

**Workflow**
- Update notice: the header shows your version and whether a newer release is out (one GitHub request when it opens; turn it off in the ☰ menu)
- Everything is stored in your DAW project. **☰ > Save as default setup** makes new instances start with your names, colours, MIDI settings and playing preferences
- **☰ > Export / Import setup** as a file (back it up, move to another computer, share it with the band): names, MIDI settings, playing preferences, your wiring choice and My drawings
- MIDI passes through, so the dropped items and the live preview share one track and one route

Preset/scene names are entered in the plugin (double-click to rename, right-click for colour/reorder).
CueDrop-style USB sync would rely on Neural DSP's undocumented protocol, so it isn't included.

## Install (no code needed)

Download the installer or zip for your system from the [website](https://thankost.github.io/pedal-cues/) or the [Releases page](https://github.com/thankost/pedal-cues/releases/latest).

**Windows**
1. Unzip, copy `PedalCues.vst3` to `C:\Program Files\Common Files\VST3\`.
2. Re-scan plug-ins in your DAW (Reaper: *Options > Preferences > Plug-ins > VST > Re-scan*). `PedalCues.exe` is the standalone version.

**macOS** (Apple Silicon and Intel, macOS 11 or later)
1. Open `PedalCues-macOS.pkg`. It isn't notarised by Apple, so the first time macOS says it can't verify it:
   click **Done**, then *System Settings > Privacy & Security > Open Anyway*.
2. Click **Install**. The app goes to *Applications*, the VST3 and AU to `/Library/Audio/Plug-Ins/`.
3. Re-scan plug-ins in your DAW.

Prefer copying by hand? `PedalCues-macOS.zip` has the same files; see the [guide](https://thankost.github.io/pedal-cues/guide.html#1-install)
(it needs a one-time `xattr -cr` in Terminal). Coming from a zip install? Delete the old copies in `~/Library/Audio/Plug-Ins/`.

**Linux** (x86-64, Ubuntu 22.04+ / Debian 12 / Fedora and similar)
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

1. Connect your devices one of three ways (**Help > Wiring Guide** in the app shows each; one device needs no chain at all). The examples use a QC and a Whammy; a Kemper or a custom device goes where the QC is, and any second device where the Whammy is:
   - **Daisy chain:** interface **MIDI Out → QC MIDI In**, QC **MIDI Thru → Whammy MIDI In** (MIDI Thru on). Both cue tracks output to that interface MIDI Out.
   - **Separate MIDI cables:** interface **MIDI Out 1 → QC**, **MIDI Out 2 → Whammy**. QC Cues → MIDI Out 1, Whammy Cues → MIDI Out 2.
   - **QC over USB + interface:** QC on USB, interface **MIDI Out → Whammy MIDI In**. QC Cues → Quad Cortex, Whammy Cues → interface MIDI Out.
   - ⚠️ **Known QC and Kemper limitation:** MIDI Thru doesn't forward MIDI received over **USB**, so "QC / Kemper on USB, Whammy on its Thru" doesn't work. The Whammy has only a 5-pin MIDI In, so it always needs a MIDI interface output (or the Thru of a 5-pin chain).
2. Give the pedals **different MIDI channels** (defaults: QC 1 via *Settings > MIDI Settings*, not Omni; Whammy 2, see its manual) and set the same numbers in the plugin's **MIDI Setup** tab, **before** dragging cues: every clip keeps the channel it was dragged with. Click *My pedals use these channels* to hide the reminder.
3. In your DAW, enable the MIDI outputs you use (Reaper: *Preferences > MIDI Devices*).
4. Create two tracks, **QC Cues** and **Whammy Cues**, insert *PedalCues* on each, and set each track's MIDI output as above (Reaper: *I/O > MIDI Hardware Output*, leave *Send to original channels*).
5. Turn snapping on and drag QC tiles onto QC Cues and Whammy tiles onto Whammy Cues at the bars you want.

Details and pictures: [guide, section 2](https://thankost.github.io/pedal-cues/guide.html#2-connect-your-rig).

## Verify with your pedals

- **Whammy numbering:** the plugin uses the manual's 1-based numbers (Classic 1–21 on / 22–42 bypassed,
  Chords 43–63 on / 64–84 bypassed; on the Whammy DT, 43–78 are Drop Tune). If every mode arrives one step off, change *MIDI Setup → Whammy program
  numbering*. Mode names can be renamed if your chart differs.
- **QC setlist:** `CC#32` = the setlist number as the QC shows it (0 = Factory Presets). Turn on *Send setlist* when your presets are in more than one setlist (the QC page shows a reminder), and drag preset clips in again after turning it on.

## Layout

```
Source/CueModel.*        MIDI definitions for both pedals + .mid file writer
Source/State.*           ValueTree schema, defaults, setup (names, MIDI settings, preferences) save/load, My drawings
Source/Tile.*            draggable tile (external file drag + click-to-send)
Source/PluginProcessor.* MIDI passthrough, preview scheduling, host tempo, state
Source/Theme.*           colour palette + custom LookAndFeel
Source/PluginEditor.*    window, header tabs, first-run tour host
Source/QcPage.cpp        Quad Cortex page    Source/KemperPage.cpp  Kemper page    Source/CustomPage.cpp  custom MIDI devices (beta)
Source/WhammyPage.cpp    Whammy V / DT page
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

Built with [JUCE](https://juce.com), which is licensed separately (AGPLv3 / JUCE licence), and [hidapi](https://github.com/libusb/hidapi) (BSD licence option). The USB sync follows the protocol documented by [pyquadcortex](https://github.com/stokes-audio/pyquadcortex) (MIT). Quad Cortex is a trademark of Neural DSP Technologies, Kemper and Profiler are trademarks of Kemper GmbH, and Whammy is a trademark of DigiTech. This project is not affiliated with any of these companies.
