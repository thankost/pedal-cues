---
name: device-data
description: Use before adding, changing or checking any device page, pedal page or template in PedalCues (Source/Modellers.cpp, Source/Pedals*.cpp, Source/DeviceTemplates.cpp). Where to get MIDI numbers, how to cross-check them, and what every page must cover.
---

# Device MIDI data: sources and rules

Every number on a device page (Program Change scheme, CC, value, range, enumeration order) must come from a source,
and each page must expose **everything the sources document**, not just a safe subset.

## Sources, in this order

1. **The maker's official documents**: owner's manual / MIDI implementation PDF (newest revision; note the revision in
   `Profile::manual`). Roland/Boss MIDI Implementation PDFs on static.roland.com, Strymon manuals on
   strymon.net/manuals, Line 6 manuals, Fractal "MIDI for 3rd party devices" and the default CC tables, etc.
2. **midi.guide** (https://midi.guide, e.g. https://midi.guide/d/strymon/timeline/): the community MIDI chart database.
   Its data is the GitHub repo **pencilresearch/midi**, one CSV per device:
   `https://raw.githubusercontent.com/pencilresearch/midi/main/<Brand>/<Device>.csv`
   (list a brand: `https://api.github.com/repos/pencilresearch/midi/contents/<Brand>`). Columns: section, parameter_name,
   cc_msb, cc_lsb, cc_min_value, cc_max_value, nrpn_*, orientation, notes, **usage** (value meanings, e.g.
   `0: Quarter; 1: Dotted Eighth`). Licence **CC BY-SA 4.0**: use the facts, don't copy its text wholesale, and say
   "midi.guide (community chart)" in the profile's `notes` for anything that comes only from it.
3. Maker-staff forum posts or open-source code with a licence (facts only), marked as such in `notes`.

Always fetch **both** 1 and 2 and compare them. When they disagree, follow the manual and mention the difference in
`notes`. A value order that only midi.guide gives may be used if its notes don't flag it as unknown; say so in `notes`.
Never fill a gap from memory. If neither source states it, leave it out and list it in `notes` ("not documented").

## Coverage checklist (every page)

Go through the whole chart and make sure each documented control has a home:
- **Presets:** the exact PC (and bank CC#0 / CC#32) scheme and labels as the unit shows them; reserved programs
  (`reservedPrograms`: Manual mode, Favorite, Live, Bypass...).
- **Controls view:** bypass / engage, on/off switches (paired utilities with the real thresholds), tap, infinite /
  hold / freeze, modes, routing, and **footswitches** as `Control::press` tiles (value then offValue 1/16 later:
  Strymon A / B / Tap down 0 / up 127, MX press 0 / release 127). Enumerations with a known order (tap division,
  routing, LFO shape, config) as value tiles (`switches` with fixed values, or utilities).
- **Looper view:** every looper CC (any-value toggles are utilities, never repeated).
- **Expression view (`pedals`):** expression CC first, then every knob and every continuous parameter, with
  `Control::max` set to its real top value (Boost 0-60, Low end 0-20, Smear 0-18...) so moves and Set to tiles stay in range.
- **Models view (`models`):** type / machine / engine selectors, only when the order is documented.
- **Per-mode parameters** (e.g. TimeLine dTape Tape Speed, Trem LFO Shape): include them; name the mode in the tile
  ("dTape: Tape speed fast") and say in the note that they only act when that mode is loaded.
- Never include factory reset, save, clear / erase, or anything that writes presets (Chase Bliss CC#56, save CCs).
- `padCc`: if CC#0 or CC#32 is a control on the unit, pick a CC its full chart doesn't use (comment why).

## When done

- Add checks in the brand's `Tests/<Group>Checks.inc` (a few CCs, ranges, press tiles, preset labels).
- `notes` (About this unit) list what was left out and why, and which numbers came from midi.guide only.
- Then follow CLAUDE.md's "Every change that touches the UI or behaviour" list (docs, screenshots, site, changelog).
