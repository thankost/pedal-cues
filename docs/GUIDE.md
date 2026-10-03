# PedalCues user guide

PedalCues turns pedal changes into **drag and drop**. Each tile in the plugin is a Quad Cortex preset, a scene, a footswitch, the tuner, a Whammy V or Whammy DT mode, a Whammy DT Drop Tune shift, or a Whammy treadle move. Drag a tile onto your DAW timeline and it lands as a **named MIDI clip** at that bar. Press play, and your rig follows the song.

![PedalCues cues on a DAW timeline](images/timeline.png)

---

## Contents

1. [Install](#1-install)
2. [Connect your rig](#2-connect-your-rig)
3. [Set up your DAW (once)](#3-set-up-your-daw-once)
4. [First launch: the quick tour](#4-first-launch-the-quick-tour)
5. [Quad Cortex page](#5-quad-cortex-page)
6. [Build a song, step by step](#6-build-a-song-step-by-step)
7. [Whammy V / DT page](#7-whammy-v--dt-page)
8. [MIDI Setup and your setup](#8-midi-setup-and-your-setup)
9. [Troubleshooting](#9-troubleshooting)

---

## 1. Install

You don't need the source code. Download the latest version from the [PedalCues website](https://thankost.github.io/pedal-cues/) or the [Releases page](https://github.com/thankost/pedal-cues/releases/latest).

| System | Download | Put it here |
|---|---|---|
| Windows | `PedalCues-Windows.zip` | Copy `PedalCues.vst3` to `C:\Program Files\Common Files\VST3\` |
| macOS (Apple Silicon or Intel) | `PedalCues-macOS.pkg` (installer) | Open it and click **Install**: the app goes to *Applications*, the plugins to `/Library/Audio/Plug-Ins/` |
| Linux (x86-64) | `PedalCues-Linux.zip` | `PedalCues.vst3` to `~/.vst3/`, `PedalCues.lv2` to `~/.lv2/`, and the `PedalCues` standalone app anywhere you like |

On **macOS**, double-click `PedalCues-macOS.pkg`:

1. PedalCues isn't notarised by Apple, so macOS first says it can't verify the installer. Click **Done**, open *System Settings > Privacy & Security*, scroll down and click **Open Anyway** next to the PedalCues message (within a few minutes), then confirm.
2. Click **Continue** and **Install**, and enter your Mac password when asked (the plugin folders are shared by all users).
3. Done: `PedalCues.app` is in *Applications*, the VST3 in `/Library/Audio/Plug-Ins/VST3/` and the AU in `/Library/Audio/Plug-Ins/Components/`. *Customize* lets you leave out any of the three.

To update, open the newer installer the same way; it replaces the old version.

> **Installed from the zip before (v0.4.26 or older)?** Delete the old copies in your user folder, so your DAW doesn't list PedalCues twice: `~/Library/Audio/Plug-Ins/VST3/PedalCues.vst3` and `~/Library/Audio/Plug-Ins/Components/PedalCues.component` (in Finder, **Cmd+Shift+G** opens a path).
>
> If macOS still says *"PedalCues is damaged and can't be opened"*, it isn't damaged; macOS is blocking an app from the internet. Open *Terminal* and run `xattr -cr /Applications/PedalCues.app`.

#### macOS without the installer (zip)

`PedalCues-macOS.zip` has the same files to copy by hand. Clear the download flag **once, right after unzipping**, or macOS says the app *"is damaged"*: open *Terminal* and run

```bash
xattr -cr ~/Downloads/PedalCues-macOS
```

(If you unzipped somewhere else, type `xattr -cr ` and drag the unzipped folder into the Terminal window, then press Return.) Then drag `PedalCues.app` to *Applications*, `PedalCues.vst3` to `~/Library/Audio/Plug-Ins/VST3/` and `PedalCues.component` to `~/Library/Audio/Plug-Ins/Components/`.

On **Linux** (built for Ubuntu 22.04 and newer, Debian 12, Fedora and similar; not tested on every distro yet):

```bash
unzip PedalCues-Linux.zip && cd PedalCues-Linux
mkdir -p ~/.vst3 ~/.lv2 && cp -r PedalCues.vst3 ~/.vst3/ && cp -r PedalCues.lv2 ~/.lv2/
./PedalCues        # the standalone app
```

For **Sync from QC** over USB, Linux needs a one-time permission rule (MIDI cues work without it):

```bash
echo 'KERNEL=="hidraw*", ATTRS{idVendor}=="152a", TAG+="uaccess"' | sudo tee /etc/udev/rules.d/70-quad-cortex.rules && sudo udevadm control --reload-rules && sudo udevadm trigger
```

Then unplug and replug the QC's USB cable. If you try PedalCues on Linux, please [tell us how it went](https://github.com/thankost/pedal-cues/issues/new?template=problem.yml).

Finally, let your DAW find the plugin. In **Reaper**, go to *Options > Preferences > Plug-ins > VST* and click **Re-scan**; other DAWs have a similar re-scan in their plugin settings. PedalCues appears under *Instruments*.

> There's also a Standalone app (`PedalCues.exe` / `PedalCues.app` / `PedalCues` on Linux) for testing tiles against the pedal without a DAW. It only needs a MIDI port: pick it in the **Test your pedals** card on the **MIDI Setup** tab (or in the app's **Options > MIDI Output** menu), then press **Test QC** / **Test Whammy** to check each pedal. No audio device is needed. After that, click any tile's round play button and the pedal changes. For treadle moves, set your song's tempo by clicking the BPM pill ("set tempo"), and play your guitar while you test so you hear the bend.

---

## 2. Connect your rig

PedalCues sends MIDI from your DAW's tracks to your pedals. Use **two cue tracks**, **QC Cues** and **Whammy Cues**, in every setup. What changes is only the MIDI output each track sends to, and each pedal needs a working path from its track's output. Three setups work:

![Ways to connect the Quad Cortex and the Whammy](images/routing.png)

| Setup | Connections | QC Cues output | Whammy Cues output |
|---|---|---|---|
| ✅ **Daisy chain** (what we use) | Interface MIDI Out → QC MIDI In → QC MIDI Thru → Whammy MIDI In | Interface MIDI Out | Interface MIDI Out (the same one) |
| ✅ **Separate MIDI cables** | Interface MIDI Out 1 → QC MIDI In, MIDI Out 2 → Whammy MIDI In | MIDI Out 1 | MIDI Out 2 |
| ✅ **QC over USB + interface** | QC USB → computer, interface MIDI Out → Whammy MIDI In | Quad Cortex (USB) | Interface MIDI Out |
| ❌ **QC over USB, Whammy on the QC's Thru** | QC USB → computer, QC MIDI Thru → Whammy | Doesn't work (see below) | |

> **Known Quad Cortex limitation:** MIDI Thru only passes on MIDI that arrives at the QC's 5-pin **MIDI In**. MIDI sent to the QC over **USB** is not forwarded, so a Whammy on the QC's Thru never changes. Use one of the three setups above.

**QC Mini:** it runs the same CorOS and responds to the same MIDI messages, so presets, scenes, the tuner and gig view should work the same way. It hasn't been tested on a Mini yet, and the stomp tiles for footswitches E-H may behave differently there, since the Mini has four physical footswitches. If you try it, please tell us how it goes in a [GitHub issue](https://github.com/thankost/pedal-cues/issues).

**Pedal settings**
- **Quad Cortex:** *Settings > MIDI Settings*. Set a fixed **MIDI Channel** (default 1, not *Omni*). For the daisy chain, turn **MIDI Thru** on.
- **Whammy V or Whammy DT:** set its MIDI channel (default 2) as described in the pedal's manual.
- Give the two pedals **different channels**, and set the same numbers in the plugin's **MIDI Setup** tab. That way both can share one cable without reacting to each other's cues.

---

## 3. Set up your DAW (once)

The clips PedalCues makes are plain MIDI files, so it works in any DAW that can drag in a MIDI file and send a MIDI track to a hardware MIDI output. The steps below use **Reaper** (tested); the same idea works elsewhere:

| DAW | Where the track's MIDI output is | Status |
|---|---|---|
| **Reaper** | *I/O > MIDI Hardware Output* (enable the port in *Preferences > MIDI Devices*) | Tested |
| **Ableton Live** | the track's *MIDI To* | Should work, not tested yet |
| **Cubase / Nuendo** | the track's MIDI output | Should work, not tested yet |
| **Bitwig, Studio One** | the track's MIDI / hardware output | Should work, not tested yet |
| **Logic Pro** | put the clips on an *External MIDI* track | Clips work; Logic doesn't pass a plugin's MIDI out to hardware, so test with the standalone app |
| **Pro Tools** | – | Not supported (needs AAX) |

In Reaper:


1. *Options > Preferences > Audio > MIDI Devices*: enable every MIDI output you use (your interface's MIDI Out, and the Quad Cortex if it's on USB).
2. Create two tracks, **QC Cues** and **Whammy Cues**, and insert **PedalCues** on each.
3. On each track, click the **I/O (routing)** button and under *MIDI Hardware Output* choose its output from the table above. Leave it on *Send to original channels*: PedalCues already puts each cue on the right pedal's channel.
4. Drag QC tiles onto the QC track and Whammy tiles onto the Whammy track.
5. Turn **snap to grid** on so clips land exactly on bars. Optional: save the track(s) as a **track template** so every new song starts with them.

**Check it works:** click the round play button on a scene tile, and the QC should switch scene. Click one on a Whammy mode, and the Whammy's mode LED should move.

> With the daisy chain, a single track would also work, since both pedals share one cable and their channels keep the cues apart. Two tracks keep the arrangement easier to read and let you mute one pedal.

The plugin's **MIDI Setup** tab shows the same steps for each setup:

| My wiring: Separate outputs | My wiring: Daisy chain via QC |
|---|---|
| ![Settings, two cue tracks](images/settings.png) | ![Settings, daisy chain](images/settings-qc-chain.png) |

---

## 4. First launch: the quick tour

The first time you open PedalCues, a quick tour walks you through every area. It dims the window, highlights one part at a time, and explains it.

![Quick tour, welcome](images/tour-welcome.png)

- Move through it with **Next / Back** or the **arrow keys**. **Esc** or *Skip tour* closes it.
- Open it again any time from the **☰** menu in the top-right corner (the **Help** menu in the standalone app).

![Quick tour highlighting the scenes](images/tour-scenes.png)

One step explains the **Load 1A first** switch:

![Quick tour: which preset scenes act on](images/tour-target.png)

---

## 5. Quad Cortex page

![Quad Cortex page](images/quad-cortex.png)

**Presets vs scenes:** a *preset* is a whole rig on the QC (often one per song), such as *Clean Rig* or *Drop C Heavy*. *Scenes* are the parts of the song inside that preset, such as *Intro*, *Verse* and *Chorus*. Load the preset once where the song starts, then switch scenes as the song moves on.

| Area | What it does | MIDI sent |
|---|---|---|
| **Presets** (left) | Your QC presets. Click one to open it; drag it to load that preset. | `CC#0` bank page, optional `CC#32` setlist, then Program Change |
| **Loaded preset** screen | The opened preset, with its location and scene colours. Drag it like a preset. | Same as above |
| **Scenes** | 8 scenes laid out like the QC display: **A-D top, E-H bottom**. | `CC#43` = 0-7 |
| **Load 1A first** (Scenes header) | Picks which preset scene and stomp tiles act on. See [below](#which-preset-do-scenes-and-stomps-act-on). | Preset + `CC#43` / `CC#35-42`, or the CC alone |
| **Stomps** | Switch one footswitch A-H. The header switch picks whether tiles send **ON** or **OFF**. | `CC#35-42` |
| **Scenes & Stomps / Expression** | The switch under the screen picks what the lower part shows: scene and stomp tiles, or [expression moves](#expression-swells-fades-and-wah). | |
| **Expression** | Moves whatever you assign to Expression 1 or 2 on the QC: swells, fades, wah, or your own drawing. | `CC#1` / `CC#2` |
| **Utilities** | Tuner on/off and gig view mode (Preset / Stomp / Scene). | `CC#45`, `CC#47` |

**Every tile:**

- **Drag** it onto the timeline to create a named MIDI clip, for example `QC Scene D - Chorus`.
- **Click the round play button** to send it to the pedal immediately.
- **Double-click** to rename it. **Right-click** for colour, reorder, duplicate, delete, or *Send to pedal now*.

### Which preset do scenes and stomps act on?

The **Load 1A first** switch at the top right of the **Scenes** header decides this for both scene and stomp tiles (it names the preset you have open). The header hint says which way it's set.

- **On** (the default). A scene or stomp tile first loads its own preset, then switches the scene or footswitch 1/16 later. It works whatever preset the QC is on. Scene tiles read `1A > Scene B` and the Stomps header says *after loading 1A*.
- **Off: the current QC preset.** A tile sends only the scene or footswitch change, and the QC applies it to the preset it already has loaded. There's no preset reload, so no audio gap. Use this for scene changes inside a song, after a preset clip. Scene tiles read `Scene B - current preset`.

![What a scene tile does in each mode](images/preset-target.png)

The clip names show the difference on the timeline too: `QC Clean Rig > B - Verse` loads the preset first, while `QC Scene B - Verse` switches only the scene.

#### Timing: scenes right after a preset

With **Load 1A first**, the scene (or footswitch) comes **1/16 after** the preset change, so the QC has time to load the preset. It lands a sixteenth after the bar you dropped it on. Two ways to have it exactly on the beat:

- **Drop the tile 1/16 early.** Set your DAW's grid to 1/16 and drop the tile one step before the beat: the preset goes out just before, and the scene lands on the beat. The QC still gets its loading time, so this is safe even if a different preset is loaded.
- **Inside a song, switch Load 1A first off.** Once the song's preset is loaded (by a preset clip, or the first Load 1A first scene), scene tiles with Load 1A first off send only the scene change, exactly on the beat, with no reload.

The same 1/16 applies to Expression clips with Load 1A first.

> Projects saved with PedalCues 0.4.2 or older keep their old choice. If **Load 1A first** is off in yours and you want the new behaviour, switch it on once.

### Sync your presets from the Quad Cortex (USB)

Instead of typing every preset, let PedalCues read them from the pedal:

1. Connect the **QC's USB port** to the computer and switch the QC on. Name sync only works over **USB**, but your cues can still go over any cable (5-pin MIDI, the daisy chain, or USB).
2. **Quit Cortex Control**: it keeps the USB connection to itself.
3. Click **Sync from QC (USB)** under the preset list.
4. PedalCues lists your setlists with their preset counts. Tick the ones to import and check each one's **setlist number**: the number the QC shows for it (the QC doesn't report it over USB). The Factory Presets are **0**, your own setlists start at **1**, and PedalCues sends that number as CC#32 when *Send setlist* is on. The Factory Library is unticked by default.
5. Optional: tick **Also read scenes, colours and stomps for every ticked preset** (see below).
6. Click **Import**. New presets are added at their bank and slot; presets you already have at the same setlist, bank and slot are renamed. Tick *Replace my current preset list* to start fresh instead.

What it reads:
- **Always:** setlist names, every preset's **name, bank and slot**, and the **scene names, scene colours and stomp (footswitch) names of the preset that's loaded on the QC**. This only reads: nothing on the QC changes.
- **With "every ticked preset" on:** the scene names, colours and stomp names of **every** preset you import. To read a preset the QC has to load it, so PedalCues loads each one in turn (the audio cuts each time, a few seconds per preset), then goes back to the preset and scene you were on. Do this at home, not on stage. It won't start if the loaded preset has **unsaved changes** (loading another preset would throw them away), and the QC's *Recents* list may change. It never saves, creates, deletes or edits anything.

Scenes the QC leaves unlabelled keep the name they had in PedalCues. Scene colours come straight from the QC, so the tiles match the pedal.

> This uses the QC's private USB connection (the one Cortex Control uses), which Neural DSP doesn't document. It follows the community's reverse-engineering in [pyquadcortex](https://github.com/stokes-audio/pyquadcortex). Tested on a Quad Cortex; a CorOS update could break syncing until PedalCues is updated, but your MIDI cues keep working regardless. On the QC Mini it should work but isn't tested yet.

### Expression: swells, fades and wah

Click **Expression** under the preset screen. Its tiles move whatever you assign to an **expression pedal** on the Quad Cortex: a volume block for swells, a wah, a delay or reverb mix, drive, a filter. PedalCues sends `CC#1` (Expression 1) or `CC#2` (Expression 2), which is what the QC listens to for its expression pedals, so you don't need a physical pedal plugged in.

| Expression view | Draw your own |
|---|---|
| ![Expression moves on the Quad Cortex page](images/qc-expression.png) | ![Drawing an expression move](images/qc-expression-draw.png) |

**On the Quad Cortex (once per preset):** assign the parameter you want to move to **Expression 1** (or 2), the same way you would for a real expression pedal, and set its range: heel is the lowest setting, toe the highest. Save the preset; the assignment is stored in it.

**In PedalCues:**

- **Exp 1 / Exp 2** (header) picks which expression pedal the tiles move.
- **Set to** tiles put it at a fixed spot: **Heel**, **25%**, **Half**, **75%** or **Toe**. Drop one at the start of a song or right after a preset loads, so you always start from a known position.
- **Moves**, with a **Length** (1/16 to 4 bars) and a **Curve**, written in beats so they follow the project tempo:

| Move | What it does | Good for |
|---|---|---|
| **Swell In** | Heel to toe over the length | Volume swells, opening a filter |
| **Fade Out** | Toe to heel | Fading the end of a song, closing a delay |
| **Rise & Fall** | Heel to toe and back | A swell that dies away |
| **Slow Rise** | Barely moves at first, then rises quickly | Build-ups into a chorus |
| **Wah Rhythm** | Heel to toe and back on every beat | Rhythmic wah |
| **Rise to Bar** | Holds heel, then rises during the last beat | Opening up exactly on the downbeat |
| **Toe Down / Heel Down** | Jump there and hold | |

- **Draw** sketches your own move, the same way as the [Whammy's drawn moves](#draw-your-own-move), and you can save it in [My drawings](#my-drawings-save-and-reuse-your-moves), shared with the Whammy.
- **Back to heel after move** (off by default) puts the pedal back to heel when the move ends. Leave it off for a swell that should stay up.

**Which preset do they act on?** By default, the one the QC has loaded at that moment, like the Utilities: expression clips don't load a preset, and what a move does depends on that preset's own assignment (the same **Swell In** can swell the volume in one preset and open a wah in another). So place expression clips after the preset clip they belong to.

Switch on **Load 1A first** (right of the *Set to* tiles; it names the preset you have open, like the one in the Scenes header) to make every expression clip load that preset first and start its move 1/16 later, like scene tiles do. The clips are then named like `QC Clean Rig > Exp 1 Swell In 1 bar`. It's off by default because the QC may cut the sound for a moment when it reloads a preset.

> **If a preset might get changed by accident on stage** (a stray foot on the QC): keep scenes and stomps on **Load 1A first** (the default), so the next scene cue puts the right preset back. For an important expression move, either switch on **Load 1A first** in Expression, or drop a scene clip just before the move and a **Set to** tile (for example **Heel**) right after it. Listen for a short gap when the preset is re-sent; if your QC has a MIDI setting to ignore a repeated Program Change, turning it on avoids reloading a preset that's already loaded.

Expression clips go on the **QC Cues** track, like your other QC cues, and are named like `QC Exp 1 Swell In 1 bar`.

**Example: volume swells.** In a preset, add a **Volume** block after your drives (before delay and reverb, so the tails ring on) and assign its level to Expression 1 with the range from silent to full. In PedalCues, set *Length* to `2 beats` and drop **Heel** at the start of the passage, **Swell In** where you strike each chord, and **Heel** again just before the next chord. Play each chord while the volume is still at heel, and it fades in in time with the song.

> Don't let two expression clips on the same pedal overlap: both would send values and the setting would jump between them. If you also have a real expression pedal plugged in, the last value wins, so leave it alone while the clips play.

### Adding a preset by hand

1. Click **+ Preset**.
2. Type its name, setlist number, bank (1-32) and slot (A-H), exactly as the QC shows them. For example, *Setlist 1, bank 3, slot B* is shown as `SL1 | 3B`. A factory preset is setlist **0** (shown as `Factory | 3B`).
3. Click the preset in the list, then double-click each scene to give it the same name as on your QC.
4. Right-click a scene to give it the same colour you use on the pedal.

---

## 6. Build a song, step by step

This example covers a song with a clean verse, a crunchy chorus and a Whammy solo.

1. **Load the preset at bar 1.** Click *Clean Rig* in the list, then drag the **Loaded preset** screen to bar 1.
2. **Set the intro scene.** With **Load 1A first** on (the default), dragging the **Intro** scene tile to bar 1 loads the preset and the scene in one clip. With it off, drag the preset first, then the scene just after it.
3. **Mark every section.** Drag **Verse** to bar 3, **Chorus** to bar 9, **Solo** to bar 13, and so on. Each clip is named after the scene, so the arrangement reads like a setlist.
4. **Whammy mode for the solo.** On the Whammy tab, drag **Oct Up** to one beat before the solo.
5. **Treadle move.** Set *Length* to `2 bars`, then drag **Rise & Fall** to the bar where the bend starts.
   Optional: for a delay that swells into the last chorus, assign the delay's mix to Expression 1 on the QC, then drag **Swell In** (Quad Cortex page > **Expression**) a few bars before it.
6. **Test.** Press play in your DAW and watch the QC and the Whammy follow. To check a single cue, click its tile's play button.
7. **Tuner between songs.** Drop **Tuner On** at the end of the song and **Tuner Off** before the next one.

> Clips are ordinary MIDI items. You can move, copy, split or delete them like any other item. Their names come from your tiles, so renaming a scene before you drag it keeps the timeline readable.

---

## 7. Whammy V / DT page

![Whammy V page](images/whammy.png)

PedalCues works with the **DigiTech Whammy V** (5th generation) and the **Whammy DT**. Pick yours with the **Whammy V | Whammy DT** switch on the red faceplate. The faceplate then shows the pedal's name, and on the DT the [Drop Tune](#whammy-dt-drop-tune) tiles appear. Your choice is saved with your setup (*Save as default setup*, *Export setup*).

### Modes

The 21 Whammy modes are laid out like the pedal's panel (the Whammy DT has the same modes on its left knob):
- **Top row:** the **Whammy** modes, from 2 Oct Up to Dive Bomb.
- **Middle row:** each **Harmony** mode sits right below the Whammy mode that shares its row on the pedal (Oct Up/Oct Down below 2 Oct Up, and so on).
- **Bottom row:** **Detune** Shallow and Deep, on their own row as at the bottom of the pedal.

Colours match the pedal: **Whammy** (red), **Harmony** (green), **Detune** (blue). Each tile shows its Program Change number from the DigiTech manual. Drag a mode tile to switch the Whammy.

- **Chords** (Whammy V only): uses the polyphonic *Chords* program range (43-84) instead of *Classic* (1-42). The Whammy DT has no Chords mode, and on the DT those numbers select Drop Tune, so the switch is hidden there.
- **Load bypassed:** selects the mode without engaging the effect. The tile LEDs go dark to show this.
- **Heel first:** sends `CC#11 = 0` before switching, so the new mode starts from heel with no pitch jump.

### Whammy DT: Drop Tune

![Whammy DT Drop Tune](images/whammy-dt-droptune.png)

On the Whammy DT, the Modes card has a **Whammy | Drop Tune** switch. **Drop Tune** shows the pedal's right knob as two rows of tiles:

- **Shift Up** (raise your tuning): +1 to +7 semitones, +Oct, and +Oct + Dry (an octave up mixed with your dry signal, a 12-string sound).
- **Shift Down** (drop your tuning): -1 to -7 semitones, -Oct and -Oct + Dry. For example, **-1** gives E-flat tuning and **-2** a whole step down.

Drop one where the new tuning starts, for example **-2** at the start of a song in D standard. With *Load bypassed* on, a tile picks the shift but leaves it off (its LED goes dark), ready to switch on later. Drop Tune can be combined with a Whammy, Harmony or Detune mode, like on the pedal, so you can tune down and still bend with the treadle.

| Drop Tune | 1 | 2 | 3 | 4 | 5 | 6 | 7 | Oct | Oct + Dry |
|---|---|---|---|---|---|---|---|---|---|
| Shift Up, on | 43 | 44 | 45 | 46 | 47 | 48 | 49 | 50 | 51 |
| Shift Up, bypassed | 61 | 62 | 63 | 64 | 65 | 66 | 67 | 68 | 69 |
| Shift Down, on | 60 | 59 | 58 | 57 | 56 | 55 | 54 | 53 | 52 |
| Shift Down, bypassed | 78 | 77 | 76 | 75 | 74 | 73 | 72 | 71 | 70 |

These are the Program Change numbers from the Whammy DT manual. The DT's **Momentary** footswitch has no MIDI message, so it can't be sent from a clip; drop a shift tile where it should start and the same tile with *Load bypassed* where it should stop.

### Treadle moves

![Treadle moves highlighted by the tour](images/tour-treadle.png)

Treadle moves are **CC#11** automation written in beats, so they follow your project tempo. Each tile shows its curve. You can also [draw your own](#draw-your-own-move).

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

### Draw your own move

![Draw mode](images/whammy-draw.png)

When no ready-made shape fits, click **Draw** in the *Treadle moves* header.

1. **Pick the length** first. The pad's grid shows beats, with brighter lines on each bar.
2. **Drag across the pad** to draw the treadle: bottom is heel, top is toe. Draw over any part again to fix it. Hold **Shift** to snap to heel, quarter, half, three-quarter or toe.
3. **Clear** resets the pad to heel. **Smooth** rounds off sharp edges; click it again for a softer curve.
4. **Drag the *Drawn move* tile** onto the timeline where the move should start, or click its play button to try it on the pedal.

The drawing stretches to whatever *Length* you pick. *Return to heel after move* works here too; *Curve* only applies to the shapes. Click **Shapes** to go back to the ready-made moves.

#### My drawings: save and reuse your moves

Keep the moves you like and use them again in any song:

- **Save** keeps the drawing on the pad under a name, for example *Big Bend*. The tile and the clips you drag take that name (`Whammy Big Bend 1 bar`).
- **My drawings** (the list above Save) loads a saved drawing onto the pad, ready to drag or to edit. Pick **New drawing** to start from a clear pad.
- **Edit** a loaded drawing by drawing over it, or with Clear and Smooth. The tile shows *edited* until you click **Save** again, which updates that saved drawing.
- **…** has **Save as new drawing** (keep the original and save a copy), **Rename** and **Delete**.

My drawings are saved **on your computer**, not in one project, so every project, every PedalCues track and the standalone app see the same list, and updates keep them. The same list appears in the Quad Cortex page's [Expression](#expression-swells-fades-and-wah) Draw mode, so a drawing works as a treadle move and as an expression move. Clips you've already dragged onto the timeline never change when you edit or delete a drawing. To take them to another computer, use **Export setup** (it includes My drawings) and **Import setup** there, which adds them to that computer's list.

---

## 8. MIDI Setup and your setup

| Plugin (in your DAW) | Standalone app |
|---|---|
| ![MIDI Setup tab in the plugin](images/settings.png) | ![MIDI Setup tab in the standalone app, with Test your pedals](images/settings-standalone.png) |

The **MIDI Setup** tab has two cards in the plugin, and a third, **Test your pedals**, in the standalone app:

- **Your pedals** (set once, required): the MIDI channel of the Quad Cortex and of the Whammy (V or DT: pick which on the Whammy page). They must match the pedals themselves and be different from each other. Every cue is sent on these channels, whatever your wiring. **Advanced** (folded away) has *Whammy program numbering*, only for when every mode lands one position off, and *Send setlist (CC#32)*, for presets in several setlists.
- **DAW tracks:** pick **My wiring** at the top (**Daisy chain via QC** or **Separate outputs**), and the card shows the two cue tracks and their MIDI outputs for it. Not sure how to cable the pedals? **How should I wire my pedals?** opens the **Wiring guide** (also in the ☰ / **Help** menu), with the cables and signal flow for both setups and the one that doesn't work.
- **Test your pedals** (standalone app only): the MIDI port the app sends to, with **Test QC** and **Test Whammy**. They send on the channels from *Your pedals*: Test QC turns the QC tuner on and, 1.5 s later, off again (CC#45), so it opens and closes (or just closes if it was open); Test Whammy steps through **Oct Up, 5th Up and 2 Oct Up** half a second apart (Program Changes), so you see the LED move whatever mode it was on. After each click the card tells you what it sent; check that the pedal reacted.

![Wiring guide](images/wiring-guide.png)

### Your setup: save, share, start new projects with it

Your setup is your preset, scene, footswitch and Whammy names and colours, the MIDI settings above, and your playing preferences: the Whammy's **Chords**, **Load bypassed** and **Heel first**, **Return to heel** / **Back to heel after move**, and Expression's **Load 1A first**. It's stored inside each DAW project automatically. (Length and Curve change from song to song, so they stay in each project.) From the **☰** menu (the **File** menu in the standalone app):
- **Save as default setup:** new PedalCues instances start with it.
- **Load default setup:** brings it back into this project.
- **Export setup / Import setup:** a file to back up, move to another computer, or share with your band. It also carries your **wiring choice** (Daisy chain or Separate outputs) and your [saved drawings](#my-drawings-save-and-reuse-your-moves); importing adds the drawings to your list and never removes any.

Settings that belong to one computer stay there and aren't exported: the standalone app's MIDI port and tempo (port names differ between computers), *Check for updates automatically*, and whether you've seen the quick tour.

### Updates

Under the title, PedalCues shows your version and whether it's **Up to date**. When a newer release is out, it says **Update to vX.Y.Z**: click it to see what's new and download it. It says **Couldn't check** when you're offline. The round arrows next to it check again.

PedalCues asks GitHub for the latest release when it opens; nothing else is sent. To turn that off, untick **Check for updates automatically** in the **☰** menu (in the standalone app: the **PedalCues** menu on macOS, **Help** on Windows). See what changed in each version on the [What's new](https://thankost.github.io/pedal-cues/changelog.html) page (**☰ > What's new** in the plugin, **Help > What's New** in the standalone app).

### The standalone app

- **Menus:** **File** saves, loads, exports and imports your setup; **Options > MIDI Output** picks the port; **Help** has the quick tour, the user guide, the wiring guide, What's New, Report a Problem and support. On macOS, **About** and **Check for Updates** are in the **PedalCues** menu.
- **Tempo:** there's no DAW, so click the BPM pill (**set tempo**) to type, drag or **Tap** your song's tempo. It only affects how long treadle moves last when you test them.

---

## 9. Troubleshooting

| Problem | Fix |
|---|---|
| Nothing happens on the pedal | Check the track's MIDI output (Reaper: *MIDI Hardware Output*) and that the pedal's MIDI port is enabled in your DAW's MIDI preferences. Try a tile's play button. |
| Sync from QC: "the loaded preset has unsaved changes" | Reading every preset loads each one, which would lose those edits. Save (or discard) them on the QC, then sync again. Or untick "every ticked preset" to import names and the loaded preset only. |
| Sync from QC: "No Quad Cortex found" or "Couldn't open" | Connect the QC's **USB** port (not just MIDI) and switch it on. Quit **Cortex Control**, which keeps the USB connection to itself. Wait until the QC has fully started, then try again. On **Linux**, install the USB permission rule from [Install](#1-install) once. |
| Standalone app: tiles do nothing | Pick the port your pedals are on in **MIDI Setup > Test your pedals** (or **Options > MIDI Output**), then try **Test QC** / **Test Whammy**. On Windows, close your DAW first; only one program can use a MIDI port at a time. |
| Scene or stomp changes the wrong preset | **Load 1A first** is off, so tiles act on whatever preset the QC has loaded. Switch it on in the Scenes header so they load their own preset first. |
| A stomp tile changes scenes | The QC is in Scene mode, where footswitch A-H select scenes. Put the QC in Stomp mode (or drop the **Stomp Mode** tile before your stomp cues). Before v0.4.2 the Scene Mode and Stomp Mode tiles were swapped; drag those clips in again. |
| Wrong preset loads | Check setlist, bank and slot in *Edit preset*. If presets are in other setlists, turn on *Send setlist*. Setlist numbers are as the QC shows them (Factory Presets = 0). Before v0.4.28 PedalCues sent one less, so if you added 1 to work around it, set them back. |
| The scene changes a little after the beat | That's the 1/16 gap of **Load 1A first**, which gives the QC time to load the preset. Drop the tile 1/16 early, or switch Load 1A first off inside the song. See [Timing](#timing-scenes-right-after-a-preset). |
| Whammy doesn't react | If the QC is on USB and the Whammy hangs off the QC's Thru, that can't work: the QC doesn't forward USB MIDI (a known QC limitation). Use one of the [three working setups](#2-connect-your-rig). Otherwise check the cable direction (MIDI Out to MIDI In) and the channels, set the QC to a fixed channel (not *Omni*), and for the daisy chain turn on QC MIDI Thru. |
| Expression tiles do nothing | Assign the parameter to **Expression 1** (or 2) on the QC, in that preset, and pick the same **Exp 1 / Exp 2** in PedalCues. The clips must be on the **QC Cues** track, on the QC's channel. |
| Expression moves the wrong thing | It acts on the preset the QC has loaded. Put the move after the right preset or scene clip, or switch on **Load 1A first** in Expression. |
| Whammy clips do nothing | Whammy clips must be on the **Whammy Cues** track, whose output leads to the Whammy. |
| How do I update? | When the header says **Update available**, click it and choose **Download**. Close your DAW, then open the installer on macOS, or replace the plugin files the same way you [installed](#1-install) them on Windows and Linux. Your setup and projects are kept. |
| Drop Tune tiles are missing | Pick **Whammy DT** with the switch on the Whammy page's faceplate, then click **Drop Tune** in the Modes header. |
| A Whammy DT mode tile changed my tuning | The page is set to Whammy V with **Chords** on: on the DT those program numbers are Drop Tune. Switch the page to **Whammy DT**. |
| Whammy mode is one off | *MIDI Setup > Whammy program numbering > Zero-based*. |
| Clip lands between bars | Turn on snap to grid in your DAW before dropping. |
| macOS says PedalCues "is damaged and can't be opened" | It isn't damaged; macOS blocks apps downloaded from the internet that Apple hasn't notarised. Use the installer (`PedalCues-macOS.pkg`), or run `xattr -cr /Applications/PedalCues.app` in Terminal. See [Install](#1-install). |
| macOS won't open the installer ("can't be verified") | Click **Done**, then *System Settings > Privacy & Security > Open Anyway*, within a few minutes. See [Install](#1-install). |
| My DAW lists PedalCues twice (macOS) | You have an old zip install in your user folder too. Delete `~/Library/Audio/Plug-Ins/VST3/PedalCues.vst3` and `~/Library/Audio/Plug-Ins/Components/PedalCues.component`, then re-scan. |

Still stuck, found a bug or have an idea? Use **☰ > Report a problem** in the plugin (**Help > Report a Problem** in the standalone app). It opens a short form on GitHub with your PedalCues version and computer already filled in (you need a free GitHub account). You can also start from the [Help page](https://thankost.github.io/pedal-cues/help.html), or [suggest an idea](https://github.com/thankost/pedal-cues/issues/new?template=idea.yml).

Enjoying PedalCues? It's free; if you'd like to support it, you can donate via [Buy Me a Coffee](https://buymeacoffee.com/athkost), [PayPal](https://paypal.me/athkost) or [Revolut](https://revolut.me/athkost), or from **☰ > Support PedalCues** in the plugin. Completely optional.

---

PedalCues is free software by **Thanasis Kostopoulos**, released under the [MIT License](../LICENSE). Source: [github.com/thankost/pedal-cues](https://github.com/thankost/pedal-cues). In the plugin, open **☰ > About PedalCues**. Not affiliated with Neural DSP or DigiTech.
