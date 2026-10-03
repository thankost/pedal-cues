# What's new in PedalCues

Download the latest version from the [PedalCues website](https://thankost.github.io/pedal-cues/). The app shows **Update to vX.Y.Z** under its title when a new one is out.

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
