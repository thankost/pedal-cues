# PedalCues: notes for Claude

JUCE plugin (VST3 / AU on macOS / LV2 on Linux / Standalone; Windows, macOS and Linux; tested in Reaper, DAW-neutral wording elsewhere) that turns Quad Cortex, Kemper and Whammy V / DT MIDI changes, and tiles for any other MIDI device (custom MIDI devices, beta), into drag-and-drop tiles. The author is Thanasis Kostopoulos (GitHub `thankost`). The repo is public: https://github.com/thankost/pedal-cues. The download site is https://thankost.github.io/pedal-cues/, served from `docs/` on `main`.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPEDALCUES_BUILD_DOCS=ON   # once
cmake --build build                                                       # also installs VST3/AU to ~/Library/Audio/Plug-Ins
build/PedalCuesTests_artefacts/Release/PedalCuesTests                     # unit tests, must print "All tests passed"
build/PedalCuesTests_artefacts/Release/PedalCuesTests --online            # optional: real GitHub update check
build/DocShots_artefacts/Release/DocShots docs/images                     # regenerate every screenshot and diagram
```

- The shell is zsh. Unquoted `$var` does not word-split and `%G?` needs quoting, so use arrays or `bash -c` for loops.
- DocShots renders the real UI components with demo data. Update checks are answered offline there, so the header shows "Up to date".
- To check a dialog (AlertWindow) visually, temporarily add a snapshot of the modal component to DocShots, render it to the scratchpad, then restore the file.

## Every change that touches the UI or behaviour

Update everything that describes it in the same commit:
- `README.md`
- `docs/GUIDE.md` (sections, troubleshooting table, anchors used by README and the site). The website's guide page `docs/guide.html` is generated from it: after editing GUIDE.md run `python3 Tools/make_site_pages.py`
- `docs/index.html` (the website: features, gallery captions, support section)
- `docs/help.html` (Help page: troubleshooting quick answers, report/idea forms) and `docs/pages.css` (styles shared by help.html and the generated guide.html / changelog.html)
- `.github/ISSUE_TEMPLATE/` (problem.yml / idea.yml forms; the app's Report a problem prefills `version`, `os`, `daw` by field id, so keep those ids)
- the quick tour (`Source/Tour.cpp`). Inserting a step shifts the indices in DocShots' `tourShots`.
- screenshots: run DocShots into `docs/images`, then look at the changed images before committing.

Show the user screenshots before pushing a visual change.

## Commits

- Author and committer: `Thanasis Kostopoulos <26544748+thankost@users.noreply.github.com>` (set in the repo's local git config).
- Sign every commit with the SSH key `~/.ssh/thankost` (local config: `gpg.format ssh`, `commit.gpgsign true`). GitHub must show commits as **Verified**.
- **No `Co-Authored-By` trailers** and no other attribution lines.
- Push with thankost's credentials. If `gh auth status` shows another active account, run `gh auth switch -u thankost`, push with
  `git -c credential.helper= -c credential.helper='!gh auth git-credential' push ...`, then switch back.
- Rulesets protect `main` (no deletion or force-push) and `v*` tags (no deletion, moving or force-push). Only the repo admin can bypass them. Rewriting history needs the user's explicit OK.

## Releases

Use the `release` skill (`.claude/skills/release/SKILL.md`). In short:
1. Bump `project(PedalCues VERSION x.y.z)` in `CMakeLists.txt`.
2. Add the version at the top of `CHANGELOG.md`, in plain words for musicians, then run `python3 Tools/make_site_pages.py` to rebuild `docs/changelog.html` and `docs/guide.html` (never edit those pages by hand).
3. Build and run the tests.
4. Commit with a message that works as release notes: the first line is `vX.Y.Z: summary`, then `- bullet` lines. CI copies the tagged commit's message into the GitHub release, and the plugin's update window shows it under "What's new".
5. Push `main`, then tag `vX.Y.Z` and push the tag. CI (`.github/workflows/build.yml`) builds the zips and the macOS installer (`Tools/make_macos_pkg.sh`: app to /Applications, VST3/AU to /Library/Audio/Plug-Ins, non-relocatable bundles, a welcome page saying to quit the DAW, `must-close` for the app's bundle id `com.athkost.pedalcues` so Installer asks to quit a running PedalCues, not Developer-ID signed) and publishes the release. The app's Update button downloads the pkg on macOS.
6. Wait for one release to finish before tagging the next. CI marks only the highest version as Latest.
7. Confirm the release has `PedalCues-macOS.pkg`, `PedalCues-macOS.zip`, `PedalCues-Windows.zip` and `PedalCues-Linux.zip`, is Latest, and the commit shows as verified.
8. Linux: CI builds on Ubuntu 22.04 (needs libudev-dev for hidapi). Nobody has tested it on real Linux hardware yet; USB sync there needs the udev rule from the guide.

## Domain facts (verified on real gear, by the user, or from the manufacturer's manual)

- **Quad Cortex MIDI:** bank CC#0, setlist CC#32 (value = the setlist number the QC shows; **0 = Factory Presets**, per Neural DSP's docs and a user's report; sent as setlist − 1 before v0.4.28, which was off by one), then Program Change; scenes CC#43 (0-7); footswitches CC#35-42 (0 off / 127 on); tuner CC#45; **Gig View screen CC#46** (0 close / 127 open, per the QC manual; Utilities' Gig View On/Off tiles); **footswitch mode CC#47: 0 = preset, 1 = stomp, 2 = scene**. The QC manual order is wrong on the author's unit, so keep this mapping.
- CC#35-42 act like pressing the footswitch in the QC's *current* mode. In Scene mode, footswitch A selects scene A.
- **Known QC limitation:** MIDI Thru only forwards MIDI arriving at the 5-pin MIDI In, never MIDI received over USB.
- Working setups, always with two cue tracks (**QC Cues**, **Whammy Cues**) set to *Send to original channels*:
  1. **Daisy chain** (what the author uses): interface MIDI Out → QC MIDI In → QC Thru → Whammy. Both tracks send to the interface MIDI Out.
  2. **Separate MIDI cables:** MIDI Out 1 → QC, MIDI Out 2 → Whammy.
  3. **QC over USB + interface:** QC track → Quad Cortex (USB), Whammy track → interface MIDI Out.
  - Not working: QC on USB only, with the Whammy on the QC's Thru.
- **Whammy V / Whammy DT** (`whModel`: 0 = V, 1 = DT; picked only with the switch on the Whammy page's faceplate, a MIDI Setup duplicate was removed; part of *Your setup*). The tab is always "Whammy V / DT". The tour's "Got a Whammy DT?" step shows DT > Drop Tune via `showWhammyDt` and restores the user's choice after. The DT's Whammy side uses the V's Classic numbers (PC 1-21 on / 22-42 bypassed); the DT has no Chords mode, and on the DT PC 43-78 are **Drop Tune** (DT manual p.13): Shift Up 1-7, Oct, Oct+Dry = 43-51 on / 61-69 bypassed; Shift Down 1-7, Oct, Oct+Dry = 60-52 on / 78-70 bypassed (`whammy::dropTuneProgram`). So Chords is hidden on the DT. The DT's Momentary footswitch has no MIDI message. CC#11 treadle is the same on both. Drop Tune view: `whDropTuneView`.
- **Kemper** (`Source/KemperPage.cpp`, `cues::kemper`; from Kemper's MIDI Parameter Documentation, not tested on real hardware). `ampUnit` (0 = Quad Cortex, 1 = Kemper Profiler, 2 = Kemper Player) picks what the first tab shows (`makeAmpPage`); the tab is named after the unit (`ampUnitName`), Kemper green, with a ▾ menu (`amp.unit`), duplicated as the Amp modeller row in MIDI Setup. Performance mode: slot index = (perf-1)*5+slot, sent as CC#32 bank LSB (index/128) + PC (index%128); the Player has no bank, PC only, 10 banks of 5. CC#50-54 = slot 1-5 of the current performance (**Load P1 first** off, `kemperSlotFirst`). Effect switches take **1/0** (not 127): A-D CC#17-20, X 22, MOD 24, DLY 26 cut / 27 tails, REV 28 / 29 (`kemperKeepTails`). Tuner 31, tap 30, rotary 33, morph button 80. Pedals view (`kemperPedalsView`): wah CC#1, pitch 4, volume 7, morph 11, sharing the QC expression shapes. The Kemper only reports the current rig name over SysEx and no colours, so there is no sync. The Kemper's global MIDI channel defaults to Omni. **USB MIDI has no MIDI Thru** on the Kemper (Kemper staff on their forum), the same as the QC; all Profilers (OS 10.2+) and the Player take MIDI over USB. The Whammy V / DT have only a 5-pin MIDI In, no USB MIDI. The wiring guide (`WiringGuide(ampUnit)`) follows the unit.
- **Custom MIDI devices (beta)** (`Source/CustomPage.cpp`, `cues::custom`, `state::createCustomUnit/addCustomUnit/saveUnit/loadUnit`): `ampUnit` 3 (`state::customAmpUnit`) shows the selected one (`selectedCustomUnit`) of `CustomUnits > Unit { name, colour, notes, programBase } > Group > CueTile { name, colour, note, messages }`. Internal IDs say "unit"; everything the user sees says **MIDI device** (files `.pedalcues-device`, root `PedalCuesUnit`). `messages` is text like `bank 1, PC 5, CC#34 = 2` (Program Change, Control Change, bank select = CC#0), all sent together at the drop point; the guided tile editor (`TileEditor`, rows per message, starting points Preset / Bank + preset / Switch on/off / Set a value) writes it via `custom::describe`, which `custom::parse` reads back. **No wait/delay step**: it isn't MIDI and the gap a device needs is unverified; users drop a preset tile, then a scene tile just after it. Wording is device-neutral (synths, loopers too); the QC, Kemper and Whammy pages keep their guitar terms. `ui::AmpInfo` / `ampInfo (state)` gives the unit's name, wiring-box name, short name and colour for the tab, MIDI Setup and the wiring guide. The custom device uses `qcChannel` (the first unit's channel); its Test sends its first valid tile. Custom devices are part of the setup (saveLibrary/loadLibrary) and the project.
- **Wiring guide** (`WiringGuide`, `makeWiringGuide (AmpInfo, view)`): three views, One device / Daisy chain / Separate outputs, opening on the MIDI Setup wiring. The first device is the unit on the first tab; the second is a generic "Second device, e.g. a Whammy". Don't make diagrams or docs assume QC + Whammy: the Whammy is the worked example.
- Default channels: QC 1 (never Omni), Whammy 2. Clips keep the channel they were dragged with, so until the user clicks **My pedals use these channels** in MIDI Setup (per-computer flag `ui::channelsConfirmedFlag`), a reminder banner (`PedalCuesEditor::ChannelBanner`) sits above the amp and Whammy pages. DocShots sets the flag for its shots and restores it. Don't state the Whammy V channel-setting procedure from memory; point to its manual.
- Quad Cortex page has a **Scenes & Stomps | Expression** switch (`qcExpressionView`). Expression (`Source/QcExpression.cpp`) sends CC#1 / CC#2 (QC expression pedal 1/2; no physical pedal needed): Exp 1/2 switch, "Set to" tiles, shapes and Draw. Moves cards for the Whammy treadle and QC expression share `Source/MovesPanel.*` (configured by `MovesConfig`), and the CC curves share `ccMove` in CueModel.cpp. Expression assignments live in each QC preset; the "assign" steps on the QC aren't verified, so keep them generic. Expression clips act on the loaded preset unless the opt-in **Load 1A first** (`expLoadFirst`, `qc::withPresetFirst`) is on.
- **My drawings** (`state::myDrawings()`, `drawings.xml` next to the default setup): saved drawn moves shared by both Draw modes, every project and instance. Never stored in projects (sanitise strips it) or in the default setup; Export setup includes it and Import merges it (never removes). Call `state::storeMyDrawings()` after every change. Tests and DocShots must use `state::useMyDrawingsFile` / a local list so the user's own file is never touched. `sweepDrawingName` / `expDrawingName` name the drawing loaded on each pad.
- Scene and stomp tiles "Load 1A first" by default (preset PC, then the scene or footswitch 1/16 later; Expression's Load 1A first uses the same gap). A setting to shorten the gap was tried and dropped: it only helps when the preset is already loaded, where "Load 1A first off" or dropping the tile 1/16 early already work, and risks the QC missing the scene after a real preset change; with that switch off (`comboPresetScene`) they act on the current QC preset. Scenes and Expression each have one "Load 1A first" toggle, worded the same way.
- **MIDI files** (`writeMidiFile`): a cue whose events all sit on tick 0 gets its last message repeated 1/16 later, because Ableton Live refuses clips with no length. Repeats must stay harmless (same absolute value).
- Standalone app (`Source/StandaloneApp.cpp`, custom JUCE standalone): no audio inputs, no Audio/MIDI settings window. Tiles go straight to the MIDI port picked in MIDI Setup > Test your pedals or Options > MIDI Output (`PedalCuesProcessor::setDirectMidiOutput`, saved as `standaloneMidiOutput` in settings.xml). Menus: macOS app menu (About, Check for Updates, auto-check toggle), File (setup), Options (MIDI Output), Help (incl. Wiring Guide). The tempo is set by the user (`setManualBpm`, `standaloneBpm`).
- The third tab is called **MIDI Setup** (it used to be "Settings"): cards Your pedals (+ Advanced), DAW tracks (wiring switch in its header; DAW-neutral wording, Reaper as the tested example), and Test (standalone only: port + Test QC / Test Whammy). Cable instructions live in the Wiring guide dialog (`ui::showWiringGuide`, Help / ☰ menu).
- "Your setup" = names + MIDI settings + playing preferences (`state::saveLibrary/loadLibrary`, `setupProperties()`: ampUnit, kemperSlotFirst, kemperKeepTails, kpReset, qcChannel, whChannel, whModel, whPcBase, sendSetlist, comboPresetScene, whChords, whBypass, whHeelFirst, sweepReset, expReset, expLoadFirst). Export also writes the wiring flag (`setupViaQcChain`) and My drawings; Import applies the flag and merges the drawings. Per-computer settings (standalone MIDI port / tempo, update check, tour seen, channels confirmed) are never exported. Save/Load default and Export/Import live in the ☰ menu (plugin) and the File menu (standalone).

## Quad Cortex USB sync (`Source/QcUsb.*`, `Source/QcSyncDialog.cpp`)

- Client for the QC's private USB-HID protocol (VID 0x152A, PID 0x880A QC / 0x892F Mini, interface 5, 129-byte reports), via hidapi (FetchContent). Follows pyquadcortex's `docs/protocol.md` (MIT).
- `readSetlists` sends only READs, the handshake, Connection and KeepAlive (incl. `RecallPreset{READ}` + `SetlistPosition{READ}` for the loaded preset's scenes/stomps). The opt-in `scanPresets` additionally sends `SetlistPosition{UPDATE}` (recall) and `Scene{UPDATE}` (restore the scene), refuses when `PresetDirty` is true, and restores the original preset. **Never send anything else that writes: no `RecallPreset` UPDATE/SAVE (can hang the unit), no `File` without an explicit READ action (an action-less File is a save), no Grid/SceneLabel/SceneColor/Tuner/power writes.**
- Flow: Version READ → ResetCommsBuffers → Version UPDATE (cortex_control_version = the unit's CorOS) → Connection true → 12 READs (ModelRepo first) → File READ type 0 → collect one File push per folder → Connection false. KeepAlive `08 01 18 01` every 1 s. Write return values are always errors: ignore them.
- Setlists = keys under `/media/p4/Presets/` plus `/opt/neuraldsp/Factory Library`. **The QC doesn't report setlist numbers** (no order field is known): the sync dialog guesses (My Presets, then A-Z), says so in amber, and remembers the numbers the user picks per folder key (`qcSetlistNumbers` setting). Its "Send the setlist with every preset change" switch (on by default) sets `sendSetlist`. The QC page shows "Setlists not sent: turn on" when presets span several setlists with `sendSetlist` off. To find the QC's real order, run `PedalCuesTests --qc-setlists` with the QC on USB: it prints setlists in arrival order with undecoded folder fields (`Folder::arrival`, `otherFields`). Slot = ProductData.index (or array position), bank = pos/8+1, slot letter = pos%8.
- Unit tests decode real captures in `Tests/fixtures/*.hex` (from pyquadcortex, MIT). Hardware testing needs the user's QC (quit Cortex Control first).

## Links used in the app and docs

- Donations (optional, never pop up by themselves): Buy Me a Coffee `https://buymeacoffee.com/athkost`, PayPal `https://paypal.me/athkost`, Revolut `https://revolut.me/athkost`. They're shown in brand colours (#FFDD00, #0070BA, white). `.github/FUNDING.yml` drives the repo's Sponsor button. Add `github: thankost` once GitHub Sponsors is approved.
- The support text mentions the author's band **ORIA** (progressive groove metal, Thessaloniki) and bandmate Leo.
- The update check (`Source/Update.*`) reads the redirect of `https://github.com/thankost/pedal-cues/releases/latest` to find the newest tag. Don't use `api.github.com` for it: that's limited to 60 requests an hour per IP, and shared networks run out. The API is only used, best-effort, for the release notes when an update exists. The check can be turned off in Settings.
