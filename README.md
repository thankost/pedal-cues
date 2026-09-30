# PedalCues

Drag-and-drop MIDI cues for the **Neural DSP Quad Cortex / QC Mini** and the **DigiTech Whammy V**.
A VST3 / AU / Standalone plugin (JUCE). Built for Reaper, works in any DAW that accepts dragged MIDI files.

**[Download for Windows and macOS](https://thankost.github.io/pedal-cues/)**

Drag a tile onto the arrangement → a **named MIDI item** lands at the drop position (e.g. `QC Scene B - Chorus`,
`Whammy Oct Up [Chords]`, `Whammy Ramp Up 1 bar`). Click a tile's ▶ corner to send it to the pedal right away.

![PedalCues - Quad Cortex page](docs/images/quad-cortex.png)

📖 **[User guide with pictures and step-by-step instructions](docs/GUIDE.md)**. A quick tour also starts the first time you open the plugin (open it again later from the **?** button).

| Whammy V | Settings |
|---|---|
| ![Whammy page](docs/images/whammy.png) | ![Settings page](docs/images/settings.png) |

## Features

**Quad Cortex**
- Preset tiles (name, colour, setlist / bank / slot) → `CC#0` page [+ `CC#32` setlist] + Program Change
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
- Everything is stored in the Reaper project, and you can save a **default library** that new instances load
- Export/import library as XML (back it up, share it with the band)
- MIDI passes through, so the dropped items and the live preview share one track and one route

Preset/scene names are entered in the plugin (double-click to rename, right-click for colour/reorder).
CueDrop-style USB sync would rely on Neural DSP's undocumented protocol, so it isn't included.

## Install (no code needed)

Download the zip for your system from the [website](https://thankost.github.io/pedal-cues/) or the [Releases page](https://github.com/thankost/pedal-cues/releases/latest).

**Windows**
1. Unzip, copy `PedalCues.vst3` to `C:\Program Files\Common Files\VST3\`.
2. Reaper: *Options > Preferences > Plug-ins > VST > Re-scan*. `PedalCues.exe` is the standalone version.

**macOS** (Apple Silicon and Intel, macOS 11 or later)
1. Unzip. The build is not notarised by Apple, so clear the download quarantine once in Terminal:
   `xattr -cr ~/Downloads/PedalCues-macOS`
   (without this, macOS says *"PedalCues is damaged and can't be opened"*).
2. Drag `PedalCues.app` (standalone) to *Applications*. Copy `PedalCues.vst3` to `~/Library/Audio/Plug-Ins/VST3/` and
   `PedalCues.component` to `~/Library/Audio/Plug-Ins/Components/`.
3. Re-scan plug-ins in Reaper.

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

## Reaper setup

1. Create a track named **Pedal Cues** and insert *PedalCues* on it.
2. In the track's routing, add **MIDI Hardware Output → Quad Cortex** (USB MIDI port). Enable the device
   under *Preferences > MIDI Devices* first.
3. In the plugin's **Settings** tab, set the MIDI channels to match the pedals:
   - QC: *Settings > MIDI Settings > MIDI Channel*
   - Whammy: hold the Whammy footswitch while powering up, turn the knob to pick the channel, press the footswitch
   - Use **different channels** for the QC and the Whammy.
4. Connect the Whammy one of two ways (the plugin's **Settings** tab has a switch for both):
   - **Option A: Whammy via QC.** QC **MIDI Out → Whammy MIDI In**, and enable **MIDI Thru** on the QC.
   - **Option B: Whammy via audio interface.** Interface **MIDI Out → Whammy MIDI In**. Use two cue tracks: **QC Cues** (output = Quad Cortex) and **Whammy Cues** (output = interface MIDI Out).
   - Details and pictures: [guide, section 3](docs/GUIDE.md#3-set-up-reaper-once).
5. Turn snapping on and drag tiles onto the **Pedal Cues** track at the bars you want.

## Verify with your pedals

- **Whammy numbering:** the plugin uses the manual's 1-based numbers (Classic 1–21 on / 22–42 bypassed,
  Chords 43–63 on / 64–84 bypassed). If every mode arrives one step off, change *Settings → Whammy program
  numbering*. Mode names can be renamed if your chart differs.
- **QC setlist:** `CC#32` = setlist number − 1. Leave *Send setlist* off if all presets are in the active setlist.

## Layout

```
Source/CueModel.*        MIDI definitions for both pedals + .mid file writer
Source/State.*           ValueTree schema, defaults, library save/load
Source/Tile.*            draggable tile (external file drag + click-to-send)
Source/PluginProcessor.* MIDI passthrough, preview scheduling, host tempo, state
Source/Theme.*           colour palette + custom LookAndFeel
Source/PluginEditor.*    window, header tabs, first-run tour host
Source/QcPage.cpp        Quad Cortex page    Source/WhammyPage.cpp  Whammy V page
Source/SettingsPage.cpp  settings / library / Reaper setup
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

## License

Free and open source under the [MIT License](LICENSE). Copyright (c) 2026 **Thanasis Kostopoulos**. [github.com/thankost/pedal-cues](https://github.com/thankost/pedal-cues)

Built with [JUCE](https://juce.com), which is licensed separately (AGPLv3 / JUCE licence). Quad Cortex is a trademark of Neural DSP Technologies and Whammy is a trademark of DigiTech. This project is not affiliated with either company.
