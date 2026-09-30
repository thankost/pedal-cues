# PedalCues user guide

PedalCues turns pedal changes into **drag and drop**. Each tile in the plugin is a Quad Cortex preset, a scene, a footswitch, the tuner, a Whammy V mode, or a Whammy treadle move. Drag a tile onto your DAW timeline and it lands as a **named MIDI clip** at that bar. Press play, and your rig follows the song.

![PedalCues on a Reaper timeline](images/timeline.png)

---

## Contents

1. [Install](#1-install)
2. [Connect your pedals](#2-connect-your-pedals)
3. [Set up Reaper (once)](#3-set-up-reaper-once)
4. [First launch: the quick tour](#4-first-launch-the-quick-tour)
5. [Quad Cortex page](#5-quad-cortex-page)
6. [Build a song, step by step](#6-build-a-song-step-by-step)
7. [Whammy V page](#7-whammy-v-page)
8. [Settings and your library](#8-settings-and-your-library)
9. [Troubleshooting](#9-troubleshooting)

---

## 1. Install

You don't need the source code. Download the latest zip from the [Releases page](https://github.com/thankost/pedal-cues/releases/latest).

| System | Download | Put it here |
|---|---|---|
| Windows | `PedalCues-Windows.zip` | Copy `PedalCues.vst3` to `C:\Program Files\Common Files\VST3\` |
| macOS | `PedalCues-macOS.zip` | `PedalCues.vst3` to `~/Library/Audio/Plug-Ins/VST3/`, `PedalCues.component` to `~/Library/Audio/Plug-Ins/Components/` |

On **macOS** the build isn't notarised, so remove the quarantine flag once in Terminal:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/PedalCues.vst3 ~/Library/Audio/Plug-Ins/Components/PedalCues.component
```

In **Reaper**, go to *Options > Preferences > Plug-ins > VST* and click **Re-scan**. PedalCues appears under *Instruments*.

> There's also a Standalone app (`PedalCues.exe` / `PedalCues.app`) for testing tiles against the pedal without a DAW.

---

## 2. Connect your pedals

There are two ways to connect the pedals. Choose the one that suits your rig. The plugin's **Settings** tab has a switch for both and shows the matching steps.

![MIDI routing, both options](images/routing.png)

**Option A: Whammy via QC.** Use this if you only have the two pedals.
- Connect the **Quad Cortex / QC Mini** to the computer with USB.
- Run a MIDI cable from the QC's **MIDI Out** to the Whammy's **MIDI In**.
- Turn on **MIDI Thru** in the QC's MIDI settings.

**Option B: Whammy via audio interface.** Use this if your sound card or a USB MIDI interface has a 5-pin **MIDI Out**, or if your QC doesn't forward USB MIDI.
- Connect the **Quad Cortex** to the computer with USB.
- Run a MIDI cable from the interface's **MIDI Out** to the Whammy's **MIDI In**.

**Both options: channels.** Give each pedal its own MIDI channel. The defaults are QC = 1 and Whammy = 2.
- QC: *Settings > MIDI Settings > MIDI Channel*.
- Whammy V: hold the footswitch while powering up, then turn the knob to pick the channel.

---

## 3. Set up Reaper (once)

First, in *Preferences > Audio > MIDI Devices*, enable the **Quad Cortex** MIDI output. For option B, also enable your **interface's MIDI output**.

### Option A: one cue track

1. Create a track called **Pedal Cues** and insert **PedalCues** on it.
2. Click the track's **I/O (routing)** button. Under *MIDI Hardware Output*, choose the **Quad Cortex**.
3. Drag QC and Whammy tiles onto this track.

### Option B: one cue track per pedal

1. Create a track **QC Cues** and insert **PedalCues** on it.
   - Set *I/O > MIDI Hardware Output* to the **Quad Cortex**.
   - Use the plugin's Quad Cortex tab here.
2. Create a track **Whammy Cues** and insert **PedalCues** on it.
   - Set *I/O > MIDI Hardware Output* to your **interface's MIDI Out**.
   - Use the plugin's Whammy V tab here.
3. Drag QC tiles onto the QC track and Whammy tiles onto the Whammy track. Each track's play button test also goes to the right pedal.

![Settings, "Whammy via interface" steps](images/settings-interface.png)

**For both options:**
- Turn **snap to grid** on so clips land exactly on bars.
- Optional: save the track(s) as a **track template** so every new song starts with them.

---

## 4. First launch: the quick tour

The first time you open PedalCues, a quick tour walks you through every area. It dims the window, highlights one part at a time, and explains it.

![Quick tour, welcome](images/tour-welcome.png)

- Move through it with **Next / Back** or the **arrow keys**. **Esc** or *Skip tour* closes it.
- Open it again any time from the **?** button in the top-right corner or with **Settings > Show quick tour**.

![Quick tour highlighting the scenes](images/tour-scenes.png)

---

## 5. Quad Cortex page

![Quad Cortex page](images/quad-cortex.png)

**Presets vs scenes:** a *preset* is a whole rig on the QC (often one per song), such as *Clean Rig* or *Drop C Heavy*. *Scenes* are the parts of the song inside that preset, such as *Intro*, *Verse* and *Chorus*. Load the preset once where the song starts, then switch scenes as the song moves on.

| Area | What it does | MIDI sent |
|---|---|---|
| **Presets** (left) | Your QC presets. Click one to open it; drag it to load that preset. | `CC#0` bank page, optional `CC#32` setlist, then Program Change |
| **Loaded preset** screen | The opened preset, with its location and scene colours. Drag it like a preset. | Same as above |
| **Scenes** | 8 scenes laid out like the QC display: **A-D top, E-H bottom**. | `CC#43` = 0-7 |
| **Also load the preset** switch | Scene tiles load their preset first, then the scene a 1/16 later. Handy after a preset change. | Preset + `CC#43` |
| **Stomps** | Switch one footswitch A-H. The header switch picks whether tiles send **ON** or **OFF**. | `CC#35-42` |
| **Utilities** | Tuner on/off and gig view mode (Preset / Scene / Stomp). | `CC#45`, `CC#47` |

**Every tile:**

- **Drag** it onto the timeline to create a named MIDI clip, for example `QC Scene D - Chorus`.
- **Click the round play button** to send it to the pedal immediately.
- **Double-click** to rename it. **Right-click** for colour, reorder, duplicate, delete, or *Send to pedal now*.

### Adding a preset

1. Click **+ Preset**.
2. Type its name, setlist number, bank (1-32) and slot (A-H), exactly as the QC shows them. For example, *Setlist 1, bank 3, slot B* is shown as `SL1 | 3B`.
3. Click the preset in the list, then double-click each scene to give it the same name as on your QC.
4. Right-click a scene to give it the same colour you use on the pedal.

---

## 6. Build a song, step by step

This example covers a song with a clean verse, a crunchy chorus and a Whammy solo.

1. **Load the preset at bar 1.** Click *Clean Rig* in the list, then drag the **Loaded preset** screen to bar 1.
2. **Set the intro scene.** Drag the **Intro** scene tile to bar 1 as well, just after the preset clip. You can also turn on *Also load the preset* and use one clip.
3. **Mark every section.** Drag **Verse** to bar 3, **Chorus** to bar 9, **Solo** to bar 13, and so on. Each clip is named after the scene, so the arrangement reads like a setlist.
4. **Whammy mode for the solo.** On the Whammy tab, drag **Oct Up** to one beat before the solo.
5. **Treadle move.** Set *Length* to `2 bars`, then drag **Rise & Fall** to the bar where the bend starts.
6. **Test.** Press play in Reaper and watch the QC and the Whammy follow. To check a single cue, click its tile's play button.
7. **Tuner between songs.** Drop **Tuner On** at the end of the song and **Tuner Off** before the next one.

> Clips are ordinary MIDI items. You can move, copy, split or delete them like any other item. Their names come from your tiles, so renaming a scene before you drag it keeps the timeline readable.

---

## 7. Whammy V page

![Whammy V page](images/whammy.png)

### Modes

The 21 Whammy V modes are laid out like the pedal's panel:
- **Top row:** the **Whammy** modes, from 2 Oct Up to Dive Bomb.
- **Middle row:** each **Harmony** mode sits right below the Whammy mode that shares its row on the pedal (Oct Up/Oct Down below 2 Oct Up, and so on).
- **Bottom row:** **Detune** Shallow and Deep, on their own row as at the bottom of the pedal.

Colours match the pedal: **Whammy** (red), **Harmony** (green), **Detune** (blue). Each tile shows its Program Change number from the DigiTech manual. Drag a mode tile to switch the Whammy.

- **Chords:** uses the polyphonic *Chords* program range (43-84) instead of *Classic* (1-42).
- **Load bypassed:** selects the mode without engaging the effect. The tile LEDs go dark to show this.
- **Heel first:** sends `CC#11 = 0` before switching, so the new mode starts from heel with no pitch jump.

### Treadle moves

![Treadle moves highlighted by the tour](images/tour-treadle.png)

Treadle moves are **CC#11** automation written in beats, so they follow your project tempo. Each tile shows its curve.

| Move | What it does |
|---|---|
| Ramp Up / Ramp Down | Heel to toe, or toe to heel, over the length |
| Rise & Fall | Heel to toe and back |
| Dive | Slow start, then accelerates to toe (try it with *Dive Bomb*) |
| Trill | Toggles heel/toe on 1/16 notes |
| Bend to Bar | Holds heel, then bends in the last beat so it lands on the next bar line |
| Toe Down / Heel Down | Jumps and holds |

- **Length:** from 1/16 note up to 4 bars.
- **Curve:** `1.00` is linear, lower values start fast, higher values start slow.
- **Return to heel after move:** adds a `CC#11 = 0` at the end.

---

## 8. Settings and your library

![Settings page](images/settings.png)

- **MIDI channels:** match these to the pedals (see [step 2](#2-connect-your-pedals)).
- **Whammy program numbering:** if the Whammy lands one mode off, switch to *Zero-based*.
- **Send setlist (CC#32):** turn this on if your presets live in different setlists.
- **Library:** all your preset, scene, footswitch and Whammy names.
  - It is stored inside each Reaper project automatically.
  - **Save as default** makes every new PedalCues instance start with it.
  - **Export / Import** moves it to another computer or shares it with your band.

---

## 9. Troubleshooting

| Problem | Fix |
|---|---|
| Nothing happens on the pedal | Check the track's MIDI Hardware Output and that the QC's MIDI device is enabled in Reaper preferences. Try a tile's play button. |
| Wrong preset loads | Check setlist, bank and slot in *Edit preset*. If presets are in other setlists, turn on *Send setlist*. |
| Whammy doesn't react | Check the MIDI cable direction and the channels. Option A: turn on QC MIDI Thru. If that still fails, switch to option B (interface MIDI Out). |
| Whammy clips do nothing (option B) | Whammy clips must be on the **Whammy Cues** track, whose output is the interface. |
| Whammy mode is one off | *Settings > Whammy program numbering > Zero-based*. |
| Clip lands between bars | Turn on snap to grid in Reaper before dropping. |
| macOS says the plugin is damaged | Run the `xattr` command from [Install](#1-install). |

Found a bug or have an idea? [Open an issue](https://github.com/thankost/pedal-cues/issues).

---

PedalCues is free software by **Thanasis Kostopoulos**, released under the [MIT License](../LICENSE). Source: [github.com/thankost/pedal-cues](https://github.com/thankost/pedal-cues). In the plugin, open **? > About PedalCues**. Not affiliated with Neural DSP or DigiTech.
