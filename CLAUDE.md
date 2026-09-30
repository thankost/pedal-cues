# PedalCues: notes for Claude

JUCE plugin (VST3 / AU / Standalone) that turns Quad Cortex and Whammy V MIDI changes into drag-and-drop tiles. The author is Thanasis Kostopoulos (GitHub `thankost`). The repo is public: https://github.com/thankost/pedal-cues. The download site is https://thankost.github.io/pedal-cues/, served from `docs/` on `main`.

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
- `docs/GUIDE.md` (sections, troubleshooting table, anchors used by README and the site)
- `docs/index.html` (the website: features, gallery captions, support section)
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
2. Add the version at the top of `CHANGELOG.md`, in plain words for musicians.
3. Build and run the tests.
4. Commit with a message that works as release notes: the first line is `vX.Y.Z: summary`, then `- bullet` lines. CI copies the tagged commit's message into the GitHub release, and the plugin's update window shows it under "What's new".
5. Push `main`, then tag `vX.Y.Z` and push the tag. CI (`.github/workflows/build.yml`) builds the macOS and Windows zips and publishes the release.
6. Wait for one release to finish before tagging the next. CI marks only the highest version as Latest.
7. Confirm the release has `PedalCues-macOS.zip` and `PedalCues-Windows.zip`, is Latest, and the commit shows as verified.

## Domain facts (verified on real gear or by the user)

- **Quad Cortex MIDI:** bank CC#0, setlist CC#32 (value = setlist - 1), then Program Change; scenes CC#43 (0-7); footswitches CC#35-42 (0 off / 127 on); tuner CC#45; **gig view mode CC#47: 0 = preset, 1 = stomp, 2 = scene**. The QC manual order is wrong on the author's unit, so keep this mapping.
- CC#35-42 act like pressing the footswitch in the QC's *current* mode. In Scene mode, footswitch A selects scene A.
- **Known QC limitation:** MIDI Thru only forwards MIDI arriving at the 5-pin MIDI In, never MIDI received over USB.
- Working setups, always with two cue tracks (**QC Cues**, **Whammy Cues**) set to *Send to original channels*:
  1. **Daisy chain** (what the author uses): interface MIDI Out → QC MIDI In → QC Thru → Whammy. Both tracks send to the interface MIDI Out.
  2. **Separate MIDI cables:** MIDI Out 1 → QC, MIDI Out 2 → Whammy.
  3. **QC over USB + interface:** QC track → Quad Cortex (USB), Whammy track → interface MIDI Out.
  - Not working: QC on USB only, with the Whammy on the QC's Thru.
- Default channels: QC 1 (never Omni), Whammy 2. Don't state the Whammy V channel-setting procedure from memory; point to its manual.
- Scene and stomp tiles "Load 1A first" by default (preset PC, then the scene or footswitch 1/4 beat later), or act on the "Current QC preset".
- Standalone app (`Source/StandaloneApp.cpp`, custom JUCE standalone): no audio inputs, no Audio/MIDI settings window. Tiles go straight to the MIDI port picked in MIDI Setup > Test output or Options > MIDI Output (`PedalCuesProcessor::setDirectMidiOutput`, saved as `standaloneMidiOutput` in settings.xml). Menus: macOS app menu (About, Check for Updates, auto-check toggle), Options (MIDI Output), Help. The tempo is set by the user (`setManualBpm`, `standaloneBpm`).
- The third tab is called **MIDI Setup** (it used to be "Settings").

## Links used in the app and docs

- Donations (optional, never pop up by themselves): Buy Me a Coffee `https://buymeacoffee.com/athkost`, PayPal `https://paypal.me/athkost`, Revolut `https://revolut.me/athkost`. They're shown in brand colours (#FFDD00, #0070BA, white). `.github/FUNDING.yml` drives the repo's Sponsor button. Add `github: thankost` once GitHub Sponsors is approved.
- The support text mentions the author's band **ORIA** (progressive groove metal, Thessaloniki) and bandmate Leo.
- The update check (`Source/Update.*`) reads the redirect of `https://github.com/thankost/pedal-cues/releases/latest` to find the newest tag. Don't use `api.github.com` for it: that's limited to 60 requests an hour per IP, and shared networks run out. The API is only used, best-effort, for the release notes when an update exists. The check can be turned off in Settings.
