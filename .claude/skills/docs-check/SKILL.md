---
name: docs-check
description: Run before every commit that touches the UI or behaviour, before showing the author the app as "done", and as step 1 of every release. Updates and checks every place that describes PedalCues (docs, tour, screenshots, site, changelog) and reports what was updated.
---

# Docs check (PedalCues)

The author's standing instruction: a change isn't done until everything that describes it is updated and checked.
Do this without being asked, and finish by listing what you updated and what needed no change.

## 1. What changed

`git status --short` and `git diff --stat` (plus `git diff HEAD~N` for already-committed work in this release).
Write down every user-visible change: features, renamed buttons / tabs / settings, new devices or brands, removed things.

## 2. Update each place (only what the change touches, but look at every one)

| Place | What to check |
|---|---|
| `README.md` | intro brand list, features, screenshots table, DAW setup steps, file map |
| `docs/GUIDE.md` | the section for each change, contents and anchors, troubleshooting table; then `python3 Tools/make_site_pages.py` (rebuilds `docs/guide.html` and `docs/changelog.html`; never edit those by hand) |
| `docs/index.html` | h1 headline, `<title>`, meta and og descriptions, feature cards, gallery captions, `data-version` / `data-released` / `data-downloads` at release |
| `docs/help.html` | quick answers that mention the changed feature or wording |
| `Source/Tour.cpp` | steps that describe it (inserting a step shifts `tourShots` in Tools/DocShots.cpp) |
| `Tools/DocShots.cpp` | a shot for each new page / view / dialog that the docs show; remove shots of removed UI |
| `docs/images` | `build/DocShots_artefacts/Release/DocShots docs/images`, then **look at** every changed image; delete images nothing references |
| `CHANGELOG.md` | the next version's section ("(unreleased)" until the release), plain words for musicians |
| `.github/ISSUE_TEMPLATE/` | device / wiring dropdowns (keep the ids `version`, `os`, `daw`) |
| `CLAUDE.md` | facts about the changed code (names, ids, rules) |

## 3. Look for stale wording

Grep README.md, docs/ (not the generated html), Source/ and Tests/ for old names the change replaced
(tabs, buttons, settings, device names), and for images referenced but missing / present but unreferenced:
`grep -o 'images/[a-z0-9-]*\.png' docs/GUIDE.md README.md docs/index.html | sort -u` vs `ls docs/images`.

## 4. Verify

`cmake --build build`, `build/PedalCuesTests_artefacts/Release/PedalCuesTests` prints "All tests passed".

## 5. Report

A short list: each place, "updated (what)" or "no change needed". Then show the author the app / screenshots
before pushing a visual change (CLAUDE.md).
