# What's new in PedalCues

Download the latest version from the [PedalCues website](https://thankost.github.io/pedal-cues/). The app shows **Update to vX.Y.Z** under its title when a new one is out.

## 0.9.1 (unreleased)

- **Import MIDI... in Draw (new):** load a move from a MIDI file, like a treadle bend or an expression swell you drew as CC automation in your DAW, and reuse it in any song. Click **Import MIDI...** next to Wave... (or drop the .mid file on the card): it keeps the clip's timing, then saves it in My drawings. In every Draw mode: the Whammy, the Quad Cortex, Kemper and modeller expression, and your own devices.
- **Moves up to 8 bars:** Length now goes to 6 and 8 bars.
- **Import from HX Edit (new):** on the Helix, HX Stomp and HX Effects pages, read preset names, setlists and snapshot names and colours from an HX Edit export (.hls, .hlb or .hlx). It only reads the file.
- **Read preset names from the unit (new, beta):** Strymon TimeLine / BigSky / Mobius, Boss DD-500 / RV-500 / MD-500 and the GT-1000, Axe-Fx III, FM9, FM3 and VP4 devices can fill in their preset names over MIDI. PedalCues only sends read requests; nothing on the unit changes. Not tested on hardware yet: please tell us if it works.
- **54 new pedal pages (new, beta)** on Effects & Pedals, built from the makers' MIDI charts: **Strymon** (TimeLine, BigSky, Mobius, TimeLine MX, BigSky MX, Volante, Sunset, Riverside, Compadre, Iridium, cloudburst, the V2 series, Brig, Olivera, Ultraviolet, Zelzah), **Boss** DD-500, RV-500, MD-500, **Meris** (Mercury7, Ottobit Jr., Polymoon, Enzo, Hedra, LVX, MercuryX, Enzo X, Ottobit X), **Chase Bliss** (MOOD MKII, Blooper, Dark World, Thermae, Habit, CXM 1978, Preamp MKII, Gen Loss MKII, Lossy, Brothers AM, Onward, Clean), **Walrus Audio** MAKO D1 / R1 / M1 / ACS1, **Source Audio** Nemesis / Ventris / Collider and **EHX** POG3 / Oceans Abyss. Presets, controls, loopers, expression moves on every knob, and model / type lists where the maker documents them. Not tested on hardware yet: please tell us what works.
- **New templates:** Eventide H9 and H90, Boss RC-500 and RC-600 (you assign the CCs on the pedal; the preset tiles work right away).
- **Delete your devices from the list:** an **x** next to each of your MIDI devices (or right-click), with a check first. Clips already in your songs keep working.
- **Effects & Pedals always shows a device:** new setups and deleted pedals show the Whammy V again instead of an empty tab.
- **Pages show only the views they have:** no greyed-out Looper or Expression buttons, and utilities sit right under the switches.
- **How to connect:** one cue track per device, "add the next one the same way".
- **Fix:** on pages without scenes (effect pedals, the Nano Cortex), on/off tiles no longer load a preset first.

## 0.9.0 (2026-10-06)

- **No more MIDI Setup tab:** each page now has a strip at the top with its device's **MIDI channel**, a **Test** button and **How to connect**. How to connect opens one window with how your devices are connected, the cue tracks to make in your DAW (with your devices as examples), your DAW, the wiring guide and, in the standalone app, the MIDI port. A MIDI port you plug in while it's open shows up by itself.
- **Every device keeps its own MIDI channel:** the Quad Cortex, QC Mini, Kemper, each Fractal / Line 6 / HeadRush / Darkglass page, each of your devices, the Whammy V and the Whammy DT. Switching devices never changes another one's channel. The channels are saved with your setup (Export setup, Save as default). Your songs and older setups keep the channels they had.
- **New setups start with No pedal** on Effects & Pedals, so you only see the devices you pick. Setups and projects that use the Whammy keep it.

## 0.8.8 (2026-10-05)

- **Two tabs, Amps & Modellers and Effects & Pedals (new):** the first tab is your amp modeller, the second a pedal next to it. The second tab still shows the Whammy V / DT, and its new **▾** lets you pick another pedal, one of your MIDI devices (a delay, a looper...), or **No pedal** if you only have one device. Each pedal keeps its own MIDI channel. Each list only shows what belongs there (pedal templates like the VP4 are on the second tab), and a device's **...** menu moves it to the other tab. Your Whammy setups and songs are unchanged.
- **Line 6 DL4 MkII and HX One (new, beta):** ready-made pages on the Effects & Pedals tab. DL4 MkII: presets A-F and 7-128, note value, bypass, tap, Classic Looper mode, routing, the looper, expression and knob moves, and every delay and reverb model. HX One: presets 000-127, ON / FLUX, engage / bypass, tap, tuner, the looper, expression and parameter moves. From Line 6's manuals, not tested on hardware yet.
- **Whammy V and Whammy DT are separate devices** in the Effects & Pedals list (the switch on the Whammy page is gone). Same page, same songs; the DT adds Drop Tune.
- **A simpler MIDI Setup:** one row per tab with the device and its channel, and a note that the Quad Cortex and Whammy are only examples until you pick yours. With two devices, choose how they're connected (each on its own output, or one cable through the first one's MIDI Thru); with one, there's nothing to choose. The daisy chain now says up front that it needs a MIDI cable into your amp modeller, not USB, and what your unit does with USB MIDI. The steps name an amp modeller track and a pedal track, with your devices as examples. Pick your DAW to see where it sets a track's MIDI output. In the standalone app, switch on the devices to check and click **Test selected** to test them all at once.

## 0.8.7 (2026-10-05)

- **Quad Cortex Looper X (new):** a **Looper** view on the Quad Cortex page (and the QC Mini): record / overdub, play / stop, undo / redo, one shot, reverse, half speed, duplicate and punch in / out, plus quantize, duplicate and clock-start settings and opening the looper screen. Drop a tile where the looper should act, like pressing its footswitch. The preset needs a Looper X block on its grid.

## 0.8.6 (2026-10-05)

- **Neural DSP Nano Cortex (new):** pick it under Neural DSP in the device list. Your 64 presets, the slots (gate, capture, cab/IR, FX 1-5) on or off, tuner, tap and expression moves. The Nano has no MIDI Out, so the wiring guide shows a second pedal on its own output.
- **Darkglass Anagram (new, beta):** presets 01A-42C, scenes A-C, footswitches A-C, tuner, Preset / Stomp / Scene mode, the looper, and moves on the expression pedal and knob bindings. From Darkglass's KosmOS 1.17 manual; if 01A doesn't load or the scenes don't switch on your Anagram, please tell us.
- **Darkglass Microtubes Infinity template (new, beta):** distortion modes, cab sim and control tiles, and expression moves on Drive or any control. Darkglass doesn't publish its MIDI chart, so this one comes from a community chart (the card says so). Its MIDI jack is TRS Type B.
- **Darkglass Infinity 500 Combo and Exponent 500 (new, beta):** pages for both amps: presets 1-5, bypass and mute, the Combo's effects, drive modes and IR slots, the Exponent's footswitches, and expression moves on every control (drive, EQ, Quick-Pots, master volume...). From their manuals' default MIDI mapping.
- Clips for units that use CC#0 as a control (the Darkglass amps, the Infinity's compression) no longer get a spare CC#0 added for Ableton.

## 0.8.5 (2026-10-04)

- **Wave... in Draw (new):** generate a sine, triangle, square or saw instead of drawing by hand, like Reaper's CC LFO. Set how many waves, the phase, the shape, the range (low / high), and let it **grow** (build up or die away) or **speed up / slow down** across the move. The wave lands in the drawing pad, so you can still fix it by hand or save it in My drawings. On the Whammy treadle and every expression pedal (Quad Cortex, Kemper, Fractal, Line 6, HeadRush, custom devices). Thanks for the idea!
- Drawings are smoother: twice as many points. Your saved drawings convert by themselves.

## 0.8.4 (2026-10-04)

- **Older Macs:** PedalCues now runs on Intel Macs from **macOS 10.13 High Sierra** (it needed macOS 11 before). Apple Silicon Macs need macOS 11, as before, and nothing changes for them. Not tested on an older Mac yet: please tell us how it goes.

## 0.8.3 (2026-10-04)

- **A Windows installer (new):** download `PedalCues-Windows-Setup.exe` and run it. The VST3 goes to the standard plugin folder, the app to *Program Files* with a Start menu entry, and a newer installer replaces the older version (it asks you to close PedalCues or your DAW if they're open). Uninstall it from *Settings > Apps*. The zip is still there if you prefer copying by hand.
- **Linux: one-command install:** the zip now has `install.sh`. Run `./install.sh` to install or update (VST3, LV2 and the app, with a menu entry), or `./install.sh --uninstall` to remove it.
- The app's **Update** button now downloads the installer on Windows, and explains the `install.sh` step on Linux.

## 0.8.2 (2026-10-04)

- **The device list is in a clearer order:** Neural DSP, Kemper, Fractal Audio, Line 6, HeadRush, Boss, then your own MIDI devices.
- The quick tour's first step after the welcome is now **Choose your device**: it shows where to pick what you play through, from a Quad Cortex to any MIDI device.

## 0.8.1 (2026-10-04)

- **Boss GT-1000 / GT-1000CORE template (new, beta):** pick it in the device list. Preset tiles follow the GT-1000's PROGRAM MAP (BANK1 PC#1, PC#2...), the Expression view starts on CC#11, and switch tiles send CC#80-83 on / off. **About this unit** says what to set on the GT-1000: MAP SELECT, the PROGRAM MAP and an ASSIGN for each CC.
- **Any device with its own MIDI mapping:** units where you set up the MIDI yourself are now marked **template** in the device list (Axe-Fx III, FM9, FM3, VP4, Boss GT-1000), and the guide and website explain how templates and your own devices cover any MIDI gear.
- About and the website now say that all product and company names are trademarks of their respective owners.

## 0.8.0 (2026-10-04)

- **HeadRush (new, beta):** pick your unit from the device list. **Core, Prime and Flex Prime** get a page with your rigs (by their MIDI PROG number), scenes, block on/off toggles, tuner, tap, rig and footswitch modes, looper and expression moves. **Pedalboard, Gigboard and MX5** get rigs, blocks, tap and looper. Built from HeadRush's user guides and not tested on hardware yet: **About this unit** tells you what to set on the unit.
- **Quad Cortex Mini (new):** pick **Quad Cortex Mini** in the device list. Its scenes and footswitches show as A-D on **Page I** and **Page II**, like on the Mini (Page II is the big QC's E-H, the same MIDI), plus **Page I / Page II** tiles. The wiring guide mentions its small TRS MIDI jacks.
- **Expression moves for any MIDI device (new):** on a custom MIDI device, switch from **Tiles** to **Expression** for swells, fades, wah, Set to tiles and your own drawn moves on the CC you pick (CC#11, the MIDI standard Expression, by default). Great for a Boss GT-1000 or anything with an assignable expression CC. The CC is saved with the device when you share it.
- **Fixed: some single-message clips were sent twice.** To give a clip a length for Ableton, PedalCues repeated its message 1/16 later. For messages that toggle, that undid them: a **Tap** clip tapped twice, **Line 6 footswitch** and **tuner** clips pressed twice, **Fractal Next / Previous scene** skipped two scenes, and custom CC tiles were sent twice. These clips now end with a harmless bank select instead. Drag those clips into your DAW again.
- **Fixed:** with the MIDI channel reminder showing in a small window, scene tiles could get too short to show their names.
- **GitHub Sponsors** is now in the app's **Support PedalCues** window, next to the other (optional) ways to support it.

## 0.7.0 (2026-10-04)

- **Fractal Audio and Line 6 (new, beta):** pick your unit from the device list. **Helix Floor, LT and Rack, HX Stomp, HX Stomp XL, HX Effects, POD Go, Helix Stadium, Axe-Fx II / XL / XL+, AX8 and FX8** each get a page like the Quad Cortex: your presets (with setlists or banks as the unit shows them), scenes or snapshots named per preset, footswitches or blocks, tuner, tap, looper and expression moves. Everything is built from the manufacturers' manuals and not tested on hardware yet: each page says so, and **About this unit** tells you what to check.
- **Axe-Fx III, FM9, FM3 and VP4 (beta):** these have no default MIDI numbers, so they come as editable MIDI devices: preset tiles ready to use, and scene, tuner and looper tiles with suggested numbers to set on your unit.
- **A searchable device list:** the arrow on the first tab opens a list you can search ("helix", "axe"), with the Quad Cortex, Kemper, Fractal, Line 6 and your own MIDI devices.
- **Quad Cortex tap tempo (new):** a **Tap** tile in Utilities (CC#44). One tile is one tap: drop one on each beat.
- **MIDI Setup is simpler:** the device is picked from the same searchable list as the first tab, and *Whammy program numbering* moved to the Whammy page (the **...** in the Modes header).
- **Switch to the preset's setlist** is now on the Quad Cortex page, under the preset list, like on the Helix, POD Go and Stadium pages. Hover it to see what it does.
- **Fixed:** clicking the Whammy or MIDI Setup tab could open the device list.
- **Fixed:** a preset clip could send its Program Change twice (the second one, 1/16 later, gives the clip a length for Ableton), and some units reload the preset when that happens. Now the clip ends with a bank select instead of a second Program Change.

## 0.6.3 (2026-10-03)

- **Search your presets (new):** type in the box above the preset list to find one fast. It's forgiving: letters in order (`drpc` finds *Drop C Heavy*), one typo (`hevy`), several words, or a location like `SL2` or `3B`. Return opens the best match, Esc clears. The Kemper's performance list and custom MIDI devices have the same search.
- **Send setlist is easier to find, and clearer:** it's now called **Switch to the preset's setlist** and sits in **MIDI Setup > Your pedals**, right under the Quad Cortex channel (it was in Advanced).
- The window keeps the keyboard until you click into a search box, so your DAW's keys (like the space bar) keep working.

## 0.6.2 (2026-10-03)

- **Fixed: presets from another setlist didn't load.** PedalCues only sends the setlist when *Send setlist* is on, and it was off by default, hidden in MIDI Setup > Advanced. **Sync from QC now turns it on** (a new switch in the sync window, on by default), and the Quad Cortex page shows a **Setlists not sent: turn on** button when your presets are in more than one setlist. Drag preset clips you made before into your DAW again: clips keep the settings they were dragged with.
- **Setlist numbers in Sync from QC:** the Quad Cortex doesn't report its setlist numbers over USB, so the sync window now says clearly that it guesses them, and **remembers the numbers you set**, so you only fix them once. Numbers go up to 32.

## 0.6.1 (2026-10-03)

- **Any MIDI device (beta, new):** not on a Quad Cortex or Kemper? Choose **New MIDI device...** under the **▾** on the first tab, and make your own tiles for a Fractal, a Helix, a Boss, a synth, a looper, or anything else that takes MIDI. Name your groups (presets, scenes, snapshots, switches...) and tiles; they drag onto the timeline like every other tile.
- **Guided tile editor:** pick each message (Program Change, Control Change or bank select) and set its numbers with − / +, or start from **Preset**, **Bank + preset**, **Switch on/off** or **Set a value**. A line in plain words says what the tile sends, and **Test on the device** tries it before you save.
- **Notes and sharing:** write down why a device is set up the way it is, on the device and on each tile. **Export device** saves it all as a file, so one person sets it up and everyone with the same gear imports it.
- **Wiring guide for any setup:** it now shows three setups: one device on its own, a daisy chain with a second device on its MIDI Thru (for example a Whammy), and separate outputs. The user guide explains wiring the same general way.

## 0.5.1 (2026-10-03)

- **Gig View On / Off (new):** two Quad Cortex tiles in Utilities open and close the QC's Gig View screen (CC#46). The Preset, Scene and Stomp Mode tiles are still there for the footswitch mode.
- **Set your MIDI channels first:** every clip keeps the channel it was dragged with, so until you confirm your channels, a reminder above the pages takes you to MIDI Setup. Click **My pedals use these channels** there once to hide it.
- Long utility tile names now fit their tile.

## 0.5.0 (2026-10-03)

- **Kemper support (new):** the first tab now works with a **Quad Cortex**, a **Kemper Profiler** (Head, Rack, Stage and the other Profilers, in Performance mode) or a **Kemper Player**. Click the **▾** on the tab (or *MIDI Setup > Amp modeller*) to pick yours; the tab is named after it.
- **Kemper page:** your performances with their five slots, like QC presets and scenes. Slot tiles load their performance first, or switch a slot of the performance already loaded. Effect tiles switch modules A–D, X, MOD, Delay and Reverb on or off (with or without tails), plus Tuner, Tap x4, Morph and Rotary tiles.
- **Kemper pedal moves:** swells, fades, wah rhythms or your own drawing on the Kemper's Wah, Pitch, Volume or Morph pedal, tempo-synced. No physical pedal needed. Built from Kemper's MIDI documentation; if you try it on your Kemper, please tell us how it goes.
- **Wiring guide follows your unit:** with a Kemper it shows the Kemper's cables. It also makes clear that the Whammy needs a MIDI cable from an interface (it has no USB MIDI), and that neither the QC nor the Kemper passes USB MIDI on to its MIDI Thru.
- **macOS installer:** if the PedalCues app is open, the installer now asks you to quit it first, so it's never replaced while running. A short welcome page reminds you to quit your DAW too (plugins it has loaded keep the old version until it restarts) and to remove old zip installs.
- Your amp modeller and the Kemper preferences are part of your setup (Save as default, Export / Import). The quick tour shows where to pick your unit.

## 0.4.29 (2026-10-03)

- **Whammy DT support (new):** pick **Whammy V** or **Whammy DT** with the new switch on the Whammy page. On the DT, a **Drop Tune** view adds Shift Up and Shift Down tiles: 1 to 7 semitones, an octave, and octave + dry, on or bypassed, so you can drop or raise your tuning right where a song needs it. The Whammy modes and treadle moves work the same on both pedals. The tab is now called **Whammy V / DT**, and the quick tour has a step for the DT.
- The Whammy model is part of your setup (Save as default, Export / Import).
- MIDI Setup is only about setup now: the *Show quick tour* and *Open user guide* buttons are gone (they're in the ☰ menu, or the Help menu in the standalone app).

## 0.4.28 (2026-10-03)

- **Fixed: setlists were one off.** With *Send setlist* on, PedalCues sent the setlist one lower than the QC expects (the QC counts the Factory Presets as 0). Setlist numbers are now exactly as the QC shows them, and Sync from QC numbers them that way. If you added 1 to your setlist numbers to work around it, set them back.
- **Fixed: some tiles wouldn't drop into Ableton Live** (a scene with Load 1A first off, the tuner and the other utilities). Those clips now have a length Ableton accepts.
- **Guide: scenes exactly on the beat.** Load 1A first sends the scene 1/16 after the preset, so the QC has time to load it. The guide now shows two ways to land exactly on the beat: drop the tile 1/16 early, or switch Load 1A first off inside a song.

## 0.4.27 (2026-10-03)

- **macOS installer (new):** download `PedalCues-macOS.pkg`, open it and click Install. The app goes to Applications and the VST3 and AU plugins to `/Library/Audio/Plug-Ins/`, with no Terminal step and no copying. The first time, macOS asks you to allow it in *System Settings > Privacy & Security > Open Anyway* (PedalCues isn't notarised by Apple). The zip is still there if you prefer copying by hand.
- On macOS, the app's **Update** button now downloads the installer.
- Coming from a zip install? Delete the old copies in `~/Library/Audio/Plug-Ins/` so your DAW doesn't list PedalCues twice.

## 0.4.26 (2026-10-03)

- **Fixed:** on macOS 27, PedalCues could quit while connecting to the Quad Cortex over USB the second time: syncing again, or reading scenes, colours and stomps for every ticked preset. Each sync now starts a fresh USB connection.

## 0.4.25 (2026-10-02)

- **Your setup now includes your playing preferences:** the Whammy's Chords, Load bypassed and Heel first, Return to heel / Back to heel after move, and Expression's Load 1A first. They come along with **Save as default setup** and **Export / Import setup**.
- **Export setup** also carries your wiring choice (Daisy chain or Separate outputs) and My drawings, so moving to another computer brings everything over. Computer-specific settings (the standalone app's MIDI port and tempo, update checks) stay on each computer.

## 0.4.24 (2026-10-02)

- **My drawings (new):** save your drawn moves by name and reuse them in any song. In Draw mode, **Save** keeps the drawing, the **My drawings** list loads one to use or edit (the tile says *edited* until you save again), and **…** saves a copy, renames or deletes. The same list works for Whammy treadle moves and Quad Cortex expression moves, and clips take the drawing's name (`Whammy Big Bend 1 bar`).
- Your drawings are saved on your computer, so every project and the standalone app see them and updates keep them. **Export setup** now includes them; **Import setup** adds them to your list.

## 0.4.23 (2026-10-02)

- Scenes header: the **Load 1A first / Current QC preset** buttons are now one **Load 1A first** switch, the same as in Expression. Off means scenes and stomps act on the preset the QC has loaded; the header says which way it's set.

## 0.4.22 (2026-10-02)

- **Expression: Load 1A first (optional).** Tick it and every expression clip first loads the preset you have open, then makes its move a 1/16 later, so it lands on the right preset even if one was changed by accident on stage. Off by default, because the QC may cut the sound for a moment when it reloads a preset. The guide has tips for staying on the right preset and scene.

## 0.4.21 (2026-10-02)

- **Quad Cortex expression automation (new):** the Quad Cortex page has a **Scenes & Stomps | Expression** switch. Expression moves whatever you assign to Expression 1 or 2 on the QC (CC#1 / CC#2): volume swells, fades, a wah rhythm, a delay mix that opens into the chorus. Ready-made moves (Swell In, Fade Out, Rise & Fall, Slow Rise, Wah Rhythm, Rise to Bar), fixed positions (heel, 25%, half, 75%, toe), or draw your own. Tempo-synced, and no expression pedal needs to be plugged in. The guide has a volume-swell example.

## 0.4.20 (2026-10-02)

- **The user guide, What's new and Help now have their own pages on the website**, instead of opening files on GitHub. The guide has a contents sidebar and Copy buttons for the Terminal commands.
- **☰ > What's new** (Help > What's New in the standalone app) opens the list of changes in every version.
- **☰ > Report a problem** (Help > Report a Problem) opens a short form with your PedalCues version and computer already filled in. There's also a form to suggest an idea.
- Wording: the guide and site no longer read as if PedalCues only works in Reaper.

## 0.4.19 (2026-10-02)

- Clearer wiring pictures (website, guide and the app's Wiring guide): links are labelled "to interface", "MIDI cable" and "QC USB", so it's obvious which connection is the QC's own USB port.

## 0.4.18 (2026-10-01)

- **Linux (new):** VST3, LV2 and the standalone app, for Ubuntu 22.04+, Debian 12, Fedora and similar (x86-64). Sync from QC works too, after a one-time USB permission rule (see the guide). Not tested on every distro yet, so feedback is very welcome.
- Website: the download counter and version now load on any network.

## 0.4.17 (2026-10-01)

- **Sync from QC now brings scenes too:** scene names, scene colours and stomp (footswitch) names. Always for the preset that's loaded on the QC, and optionally for every preset you import ("Also read scenes, colours and stomps for every ticked preset"). That option loads each preset in turn (the audio cuts), then goes back to where you were, and won't start if the loaded preset has unsaved changes.

## 0.4.16 (2026-10-01)

- **Sync from QC (USB):** read your setlists and preset names (with bank and slot) straight from the Quad Cortex, instead of typing them. Pick which setlists to import. It only reads; nothing on the pedal changes. Needs the QC's USB port connected and Cortex Control closed (the sync window reminds you in bold); your cues can still use any MIDI cable.

## 0.4.15 (2026-10-01)

- **Not just Reaper:** MIDI Setup now says **DAW tracks**, with a hint for where the MIDI output is in Reaper, Ableton Live, Cubase and Logic. The guide lists which DAWs are tested.
- **Test Whammy** steps through Oct Up, 5th Up and 2 Oct Up, so you always see the LED move, even if the Whammy was already on 2 Oct Up.

## 0.4.14 (2026-10-01)

- **MIDI Setup is simpler:** **Your pedals** (channels, with Advanced folded away) and **Reaper tracks**, with your wiring chosen right in its header. The cable instructions moved to a new **Wiring guide** (Help menu, or "How should I wire my pedals?").
- **Standalone app:** a **Test your pedals** card with **Test QC** and **Test Whammy** buttons that tell you what they sent. They're greyed out until you pick a MIDI port, and "No MIDI devices found" shows when nothing is connected.
- **Your setup in one place:** the ☰ menu (the **File** menu in the standalone app) saves your setup as the default for new projects, loads it, and exports or imports it. It now includes your MIDI channels and options, not just the names.
- The **?** button is now a **☰** menu button.

## 0.4.13 (2026-10-01)

- **Standalone app: MIDI only.** No audio setup any more and no microphone request. Pick the port your pedals are on in **MIDI Setup > Test output** or **Options > MIDI Output**, then click any tile's play button.
- **Standalone app: real menus.** On macOS, the **PedalCues** menu has About and Check for Updates, **Options** has MIDI Output, and **Help** has the quick tour, user guide and support. On Windows the same menus sit at the top of the window.
- **Standalone app: set the tempo.** Click the BPM pill to type, drag or tap your song's tempo, so treadle moves last as long as they will in the song.
- **Settings is now "MIDI Setup"**, with only the MIDI and setup options.
- The Whammy page logo reads **WHAMMY V** in one font.
- **Update check** works on busy office or venue networks too (it used to say "Couldn't check"). New look: a green tick for *Up to date*, and round arrows to check again. Turn automatic checks on or off in the **?** menu.

## 0.4.12 (2026-10-01)

- **Update notice:** the header shows your version and whether a newer release is out, with what's new and a Download button.

## 0.4.11 (2026-10-01)

- The Support window closes with an **X** in the corner.

## 0.4.10 (2026-10-01)

- Every setup now uses two cue tracks, **QC Cues** and **Whammy Cues**. The setup options are called **Separate outputs** and **Daisy chain via QC**.

## 0.4.9 (2026-10-01)

- Clearer rig setup: three ways of connecting the pedals that work, and one that doesn't (QC on USB with the Whammy on the QC's Thru).
- The website has more screenshots, and you can click them to enlarge.

## 0.4.5 to 0.4.8 (2026-10-01)

- Documented the known Quad Cortex limitation: its MIDI Thru doesn't pass on MIDI received over USB.
- Optional **Support PedalCues** window (Buy Me a Coffee, PayPal, Revolut), with the story behind the app.

## 0.4.4 (2026-09-30)

- The mode tiles are in the order **Preset / Stomp / Scene**.

## 0.4.3 (2026-09-30)

- Scene and stomp tiles clearly show which preset they act on: **Load 1A first** (default) or **Current QC preset**. New quick-tour step and guide section.

## 0.4.2 (2026-09-30)

- Fixed: the **Scene Mode** and **Stomp Mode** tiles were swapped.
- Stomp tiles can load their preset first, like scenes.

## 0.4.0 to 0.4.1 (2026-09-30)

- **Draw your own treadle moves** with the mouse.
- A properly signed universal macOS build, and clearer first-open steps.

## 0.3.0 to 0.3.2 (2026-09-30)

- The Whammy V page is laid out like the pedal, with Detune modes on their own row.
- Two setup options, a shared logo, MIT license, and example rig names.

## 0.1.0 to 0.2.2 (2026-09-29 to 2026-09-30)

- First release: drag-and-drop MIDI cues for the Quad Cortex and Whammy V, Windows and macOS builds.
- Pedal-themed UI, first-run quick tour, user guide with screenshots, app icon.
