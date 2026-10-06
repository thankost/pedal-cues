#!/usr/bin/env python3
# PreToolUse hook (Bash): a `git commit` only goes through once the docs check was done.
# The command must start with DOCS_CHECKED=1 (meaning: the docs-check skill was run for this commit).
# Anything else that isn't a commit passes untouched.
import json, re, sys

try:
    data = json.load(sys.stdin)
except Exception:
    sys.exit(0)

command = (data.get("tool_input") or {}).get("command", "")
if not re.search(r"\bgit\b[^|;&]*\bcommit\b", command):
    sys.exit(0)
if "DOCS_CHECKED=1" in command:
    sys.exit(0)

reason = """Docs check first (the author's standing rule, .claude/skills/docs-check/SKILL.md).
Before committing a change to the UI or behaviour, update and check:
- README.md; docs/GUIDE.md then python3 Tools/make_site_pages.py; docs/index.html (headline, title, meta, cards, captions); docs/help.html
- Source/Tour.cpp; Tools/DocShots.cpp shots; DocShots into docs/images and LOOK at the images
- CHANGELOG.md (next version, "(unreleased)"); .github/ISSUE_TEMPLATE; CLAUDE.md facts (never the GitHub About text)
- grep for stale names; build + PedalCuesTests "All tests passed"
Then report what was updated, and commit again with DOCS_CHECKED=1 at the start of the command
(e.g. DOCS_CHECKED=1 git commit ...). Docs-only or no-UI commits: say so and use the same prefix."""

print(json.dumps({
    "hookSpecificOutput": {
        "hookEventName": "PreToolUse",
        "permissionDecision": "deny",
        "permissionDecisionReason": reason,
    },
    "systemMessage": "Commit held for the docs check (docs-check skill)."
}))
sys.exit(0)
