#!/usr/bin/env python3
"""Convert a Markdown source file into an sblg article fragment.

sblg consumes well-formed XML "article" fragments. This script reads a Markdown
file with an optional `---`-delimited front-matter block, runs the body through
lowdown (raw HTML preserved, XML-well-formed output), and emits:

    <article data-sblg-article="1" data-sblg-title="..." ...>
      ...lowdown body...
    </article>

All article metadata is carried as attributes so the body stays clean and the
same fragment works both inlined as a full page (-c) and summarised in a
listing (<nav data-sblg-nav="1">):

  front-matter key   ->  sblg attribute                       template symbol
  ----------------       ----------------------------------   ----------------
  title              ->  data-sblg-title                      ${sblg-title}
  date               ->  data-sblg-datetime (first 10 chars)  ${sblg-date}
  tags               ->  data-sblg-tags                       ${sblg-tags}
  description        ->  data-sblg-aside + data-sblg-set-...   ${sblg-aside} / ${sblg-get|description}
  img                ->  data-sblg-img                        ${sblg-img}
  <anything else>    ->  data-sblg-set-<key>                  ${sblg-get|<key>}

Front matter is simple `key: value`, one per line (no nesting), which keeps it
trivially hand-editable.

Usage:
    mkarticle.py <input.md>            # writes fragment to stdout
    mkarticle.py <input.md> -o out.xml
"""
import subprocess
import sys

LOWDOWN = ["lowdown", "--html-no-skiphtml", "--html-no-escapehtml"]


def split_front_matter(text):
    """Return (dict_of_meta, body_str)."""
    meta = {}
    lines = text.splitlines(keepends=True)
    if lines and lines[0].strip() == "---":
        body_start = None
        for i in range(1, len(lines)):
            if lines[i].strip() == "---":
                body_start = i + 1
                break
        if body_start is not None:
            for line in lines[1:body_start - 1]:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                if ":" not in line:
                    continue
                key, val = line.split(":", 1)
                meta[key.strip()] = val.strip()
            return meta, "".join(lines[body_start:])
    return meta, text


def esc_attr(value):
    return (value.replace("&", "&amp;")
                 .replace("<", "&lt;")
                 .replace(">", "&gt;")
                 .replace('"', "&quot;"))


def render_body(markdown):
    if not markdown.strip():
        return ""
    out = subprocess.run(LOWDOWN, input=markdown, capture_output=True,
                         text=True, check=True)
    return out.stdout


def build_article(meta, body_html, source=None):
    attrs = ['data-sblg-article="1"']
    if source:
        attrs.append('data-sblg-source="%s"' % esc_attr(source))
    aside = None

    title = meta.pop("title", None)
    if title is not None:
        attrs.append('data-sblg-title="%s"' % esc_attr(title))

    date = meta.pop("date", None)
    if date:
        attrs.append('data-sblg-datetime="%s"' % esc_attr(date[:10]))

    tags = meta.pop("tags", None)
    if tags:
        attrs.append('data-sblg-tags="%s"' % esc_attr(tags))

    img = meta.pop("img", None)
    if img:
        attrs.append('data-sblg-img="%s"' % esc_attr(img))

    description = meta.pop("description", None)
    if description:
        aside = description
        attrs.append('data-sblg-set-description="%s"' % esc_attr(description))

    # Everything left over becomes a generic data-sblg-set-<key>.
    for key, val in meta.items():
        attrs.append('data-sblg-set-%s="%s"' % (key, esc_attr(val)))

    inner = ""
    if aside:
        inner += "  <aside>%s</aside>\n" % esc_attr(aside)
    inner += body_html
    if not inner.endswith("\n"):
        inner += "\n"

    return "<article %s>\n%s</article>\n" % (" ".join(attrs), inner)


def main():
    args = sys.argv[1:]
    out_path = None
    source = None
    if "-o" in args:
        i = args.index("-o")
        out_path = args[i + 1]
        del args[i:i + 2]
    if "--source" in args:
        i = args.index("--source")
        source = args[i + 1]
        del args[i:i + 2]
    if len(args) != 1:
        sys.exit("usage: mkarticle.py <input.md> [-o out.xml] [--source path]")

    with open(args[0], encoding="utf-8") as fh:
        meta, body = split_front_matter(fh.read())

    fragment = build_article(meta, render_body(body), source=source)

    if out_path:
        with open(out_path, "w", encoding="utf-8") as fh:
            fh.write(fragment)
    else:
        sys.stdout.write(fragment)


if __name__ == "__main__":
    main()
