#!/usr/bin/env python3
"""Builds the website pages that come from Markdown:

    docs/GUIDE.md  ->  docs/guide.html      (User guide)
    CHANGELOG.md   ->  docs/changelog.html  (What's new)

Run it after editing either file:  python3 Tools/make_site_pages.py
No dependencies. It understands the Markdown these two files use: headings (with GitHub-style
anchors, so links like #9-troubleshooting keep working), paragraphs, "-" and "1." lists,
tables, > quotes, ``` code blocks, ---, images, links, **bold**, *italic* and `code`.
"""
import html
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
REPO = "https://github.com/thankost/pedal-cues"


# ---------------------------------------------------------------- Markdown

def slug(text: str) -> str:
    """GitHub's heading anchor: lower case, punctuation dropped, spaces to dashes."""
    text = re.sub(r"<[^>]+>", "", text).lower()
    text = re.sub(r"[^\w\- ]", "", text)
    return text.replace(" ", "-")


def fix_link(url: str) -> str:
    """Links written for GitHub, pointed at the website (or at the repo for source files)."""
    if url.startswith(("http", "#", "mailto:")):
        return url
    if url.startswith("images/"):
        return url
    url = re.sub(r"^(\.\./)?CHANGELOG\.md", "changelog.html", url)
    url = re.sub(r"^(docs/)?GUIDE\.md", "guide.html", url)
    if url.startswith("../"):
        return f"{REPO}/blob/main/{url[3:]}"
    return url


def inline(text: str) -> str:
    codes = []

    def keep_code(m):
        codes.append("<code>" + html.escape(m.group(1), quote=False) + "</code>")
        return f"\x00{len(codes) - 1}\x00"

    text = re.sub(r"`([^`]+)`", keep_code, text)
    text = html.escape(text, quote=False)
    text = re.sub(r"!\[([^\]]*)\]\(([^)]+)\)",
                  lambda m: f'<img src="{fix_link(m.group(2))}" alt="{m.group(1)}" loading="lazy">', text)
    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", lambda m: f'<a href="{fix_link(m.group(2))}">{m.group(1)}</a>', text)
    text = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", text)
    text = re.sub(r"(?<![*\w])\*([^*\s][^*]*)\*(?![*\w])", r"<i>\1</i>", text)
    return re.sub(r"\x00(\d+)\x00", lambda m: codes[int(m.group(1))], text)


def cells(row: str):
    return [c.strip() for c in row.strip().strip("|").split("|")]


def markdown(source: str):
    """Returns (html, [(level, anchor, title)]) for the headings."""
    lines, out, toc, i = source.splitlines(), [], [], 0
    while i < len(lines):
        line = lines[i]
        if not line.strip():
            i += 1
        elif line.startswith("```"):
            lang, i, code = line[3:].strip(), i + 1, []
            while i < len(lines) and not lines[i].startswith("```"):
                code.append(lines[i]); i += 1
            i += 1
            out.append(f'<div class="code"><pre><code class="{lang}">{html.escape(chr(10).join(code))}</code></pre>'
                       f'<button class="copy" type="button">Copy</button></div>')
        elif m := re.match(r"(#{1,4}) (.+)", line):
            level, title = len(m.group(1)), inline(m.group(2).strip())
            anchor = slug(m.group(2))
            toc.append((level, anchor, title))
            out.append(f'<h{level} id="{anchor}">{title}</h{level}>')
            i += 1
        elif re.fullmatch(r"-{3,}", line.strip()):
            out.append("<hr>"); i += 1
        elif line.startswith(">"):
            quote = []
            while i < len(lines) and lines[i].startswith(">"):
                quote.append(lines[i][1:].strip()); i += 1
            paras = [inline(" ".join(p.split("\n"))) for p in "\n".join(quote).split("\n\n") if p.strip()]
            out.append("<blockquote>" + "".join(f"<p>{p}</p>" for p in paras) + "</blockquote>")
        elif line.startswith("|") and i + 1 < len(lines) and re.match(r"\|[\s:|-]+\|?$", lines[i + 1]):
            head, i, rows = cells(line), i + 2, []
            while i < len(lines) and lines[i].startswith("|"):
                rows.append(cells(lines[i])); i += 1
            if rows and all(re.fullmatch(r"!\[[^\]]*\]\([^)]+\)", c) for r in rows for c in r):
                # A table of screenshots side by side: show them as figures that fit the column.
                figs = []
                for r in rows:
                    for caption, c in zip(head, r):
                        src = fix_link(re.search(r"\(([^)]+)\)", c).group(1))
                        figs.append(f'<figure><a href="{src}">{inline(c)}</a><figcaption>{inline(caption)}</figcaption></figure>')
                out.append(f'<div class="pair">{"".join(figs)}</div>')
                continue
            out.append('<div class="table"><table><thead><tr>' + "".join(f"<th>{inline(c)}</th>" for c in head)
                       + "</tr></thead><tbody>"
                       + "".join("<tr>" + "".join(f"<td>{inline(c)}</td>" for c in r) + "</tr>" for r in rows)
                       + "</tbody></table></div>")
        elif re.match(r"(- |\d+\. )", line):
            tag = "ol" if line[0].isdigit() else "ul"
            items = []
            while i < len(lines) and (re.match(r"(- |\d+\. )", lines[i]) or (lines[i].startswith("  ") and items)):
                if lines[i].startswith("  "):
                    items[-1] += " " + lines[i].strip()
                else:
                    items.append(re.sub(r"^(- |\d+\. )", "", lines[i]))
                i += 1
            out.append(f"<{tag}>" + "".join(f"<li>{inline(t)}</li>" for t in items) + f"</{tag}>")
        else:
            para = []
            while i < len(lines) and lines[i].strip() and not re.match(r"(#{1,4} |```|>|\||- |\d+\. |-{3,}$)", lines[i]):
                para.append(lines[i].strip()); i += 1
            body = inline(" ".join(para))
            out.append(f'<p class="figure">{body}</p>' if re.fullmatch(r"<img [^>]+>", body) else f"<p>{body}</p>")
    return "\n".join(out), toc


# ---------------------------------------------------------------- pages

def page(title: str, description: str, current: str, main: str, source: str) -> str:
    nav = [("./#features", "Features"), ("./#install", "Install"), ("guide.html", "User guide"),
           ("changelog.html", "What's new"), ("help.html", "Help"), ("./#support", "Support"), (REPO, "GitHub")]   # menu and footer match index.html and help.html
    links = "".join(f'<a href="{href}"' + (' aria-current="page"' if href == current else "") + f">{name}</a>"
                    for href, name in nav)
    return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{title} - PedalCues</title>
  <meta name="description" content="{description}">
  <link rel="icon" type="image/png" href="images/icon.png">
  <link rel="stylesheet" href="pages.css">
</head>
<body>
<!-- Generated from {source} by Tools/make_site_pages.py. Don't edit by hand. -->
<header class="top"><div class="wrap">
  <a class="brand" href="./"><img src="images/icon.png" alt=""><b>PedalCues</b></a>
  <nav>{links}</nav>
</div></header>
{main}
<footer>
  <div class="wrap">
    <span>&copy; 2026 Thanasis Kostopoulos &middot; MIT License &middot; Built with JUCE &middot; <a href="guide.html">User guide</a> &middot; <a href="changelog.html">What's new</a> &middot; <a href="help.html">Help</a> &middot; <a href="https://github.com/thankost/pedal-cues">GitHub</a></span>
    <span>Not affiliated with Neural DSP or DigiTech. Quad Cortex and Whammy are trademarks of their owners.</span>
  </div>
</footer>
<script>
  document.querySelectorAll('.code .copy').forEach(function (b) {{
    b.addEventListener('click', function () {{
      navigator.clipboard.writeText(b.previousElementSibling.innerText).then(function () {{
        b.textContent = 'Copied'; setTimeout(function () {{ b.textContent = 'Copy'; }}, 1500);
      }});
    }});
  }});
</script>
</body>
</html>
"""


def guide():
    text = (DOCS / "GUIDE.md").read_text(encoding="utf-8")
    # The page has its own title and sidebar, so drop the Markdown title and its Contents list.
    text = re.sub(r"\A# .*\n", "", text)
    text = re.sub(r"## Contents\n.*?\n---\n", "", text, flags=re.S)
    body, toc = markdown(text)
    side = "".join(f'<a href="#{a}">{t}</a>' for level, a, t in toc if level == 2)
    main = f"""<div class="wrap guide">
  <aside class="toc"><b>Contents</b>{side}</aside>
  <main class="doc">
    <h1>User guide</h1>
{body}
  </main>
</div>"""
    return page("User guide", "How to install PedalCues, connect your Quad Cortex and Whammy V, and build songs with "
                "drag-and-drop pedal cues.", "guide.html", main, "docs/GUIDE.md")


def changelog():
    text = (ROOT / "CHANGELOG.md").read_text(encoding="utf-8")
    sections = re.split(r"^## ", text, flags=re.M)[1:]           # the intro before the first release is skipped
    cards = []
    for section in sections:
        title, _, rest = section.partition("\n")
        body, _ = markdown(rest)
        if m := re.match(r"([\d.]+)\s*\((.+)\)", title.strip()):
            head = f'<h2>{m.group(1)}</h2><div class="date">{html.escape(m.group(2))}</div>'
            cards.append(f'<section class="release" id="v{m.group(1)}">{head}{body}</section>')
        else:
            cards.append(f'<section class="release"><h2>{inline(title)}</h2>{body}</section>')
    main = f"""<main class="wrap narrow">
  <h1>What's new</h1>
  <p class="lead">Every release, newest first. <a href="./#download">Download the latest version</a>.</p>
{chr(10).join(cards)}
</main>"""
    return page("What's new", "Every PedalCues release and what changed, in plain words.", "changelog.html", main,
                "CHANGELOG.md")


if __name__ == "__main__":
    for name, build in (("guide.html", guide), ("changelog.html", changelog)):
        (DOCS / name).write_text(build(), encoding="utf-8")
        print(f"wrote docs/{name}")
