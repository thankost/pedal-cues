---
name: release
description: Cut a new PedalCues release (version bump, signed commit with release notes, tag, CI build, verify). Use when the user asks to release, publish, ship or bump the version.
---

# Release PedalCues

Follow these steps in order. Stop and report if any step fails.

1. **Check the tree.** Run `git status`. Everything that should ship is committed or about to be. If the UI or behaviour changed, the docs, website and screenshots must already be updated (see CLAUDE.md).

2. **Wait for the previous release.** Run `gh run list -R thankost/pedal-cues --workflow Build --limit 4`. If a tag build is still running, wait for it to finish. Releasing two versions at once can mark the wrong one as Latest.

3. **Update the website's fallback numbers.** In `docs/index.html`, on `<section id="downloads-section" ...>`, set `data-version` to the new version, `data-released` to today (YYYY-MM-DD) and `data-downloads` to the current total (`gh api 'repos/thankost/pedal-cues/releases?per_page=100' --jq '[.[].assets[].download_count] | add'`). They show until live numbers load.

4. **Update `CHANGELOG.md`.** Add a `## X.Y.Z (YYYY-MM-DD)` section at the top with the changes in plain words for musicians. Then run `python3 Tools/make_site_pages.py` to rebuild the website's generated pages: `docs/changelog.html` (What's new, opened by **☰ > What's new** in the app) and `docs/guide.html` (User guide). Commit them together.

5. **Bump the version.** Change `project(PedalCues VERSION x.y.z)` in `CMakeLists.txt`. Use the patch number for fixes and small features, and the minor number for bigger features.

6. **Build and test.**
   ```sh
   cmake --build build
   build/PedalCuesTests_artefacts/Release/PedalCuesTests
   ```
   The tests must print `All tests passed`. If screenshots changed, run DocShots into `docs/images` and look at them.

7. **Commit.** The message becomes the GitHub release notes and the plugin's "What's new":
   ```
   vX.Y.Z: one-line summary a musician understands

   - what changed, in plain words
   - another change
   ```
   The commit must be signed with `~/.ssh/thankost` and authored by thankost, with **no Co-Authored-By line**. Check with `git log -1 --format='%an <%ae> %G?'`, which must print `G`.

8. **Push and tag** as thankost:
   ```sh
   gh auth switch -u thankost
   git -c credential.helper= -c credential.helper='!gh auth git-credential' push origin main
   git tag vX.Y.Z
   git -c credential.helper= -c credential.helper='!gh auth git-credential' push origin vX.Y.Z
   gh auth switch -u <previously active account>
   ```

9. **Wait for CI.** Takes about 7-15 minutes. Run it in the background:
   ```sh
   RID=$(gh run list -R thankost/pedal-cues --branch vX.Y.Z --limit 1 --json databaseId -q '.[0].databaseId')
   gh run watch $RID -R thankost/pedal-cues --exit-status
   ```

10. **Verify, then tell the user.**
   - `gh release list -R thankost/pedal-cues --limit 3`: the new version is **Latest**.
   - `gh release view vX.Y.Z -R thankost/pedal-cues --json assets -q '.assets[].name'`: `PedalCues-macOS.pkg` (the installer), `PedalCues-macOS.zip`, `PedalCues-Windows.zip` and `PedalCues-Linux.zip` are all attached.
   - `gh api 'repos/thankost/pedal-cues/commits?per_page=1' --jq '.[0].commit.verification.verified'` prints `true`.
   - If an older version is marked Latest, run `gh release edit vX.Y.Z -R thankost/pedal-cues --latest` as thankost.
