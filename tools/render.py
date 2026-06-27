#!/usr/bin/env python3
"""Render an sblg template by substituting @@key@@ placeholders.

sblg has no template logic of its own, so all per-language UI strings and
per-page values are baked in *before* sblg runs. This script takes a template
containing @@key@@ placeholders, a monolingual JSON catalog (i18n/<lang>.json),
and any number of ad-hoc key=value overrides, and writes the rendered template
to stdout.

Usage:
    render.py <template.in> <catalog.json> [key=value ...]

  * Values come from the JSON catalog, then are overlaid with key=value args
    (the args win on conflict). This is how page-specific values such as
    section or url.self.<lang> are injected.
  * Use key@=path to read the value from a file instead (used to inject a
    pre-rendered HTML blob such as the home-page intro).
  * Placeholders match @@<key>@@ where <key> is [A-Za-z0-9_.-]+.
  * An unresolved placeholder is replaced with the empty string and reported
    on stderr, so a forgotten variable never leaks raw "@@...@@" into output
    yet is still surfaced during the build.
"""
import json
import re
import sys

PLACEHOLDER = re.compile(r"@@([A-Za-z0-9_.-]+)@@")


def main():
    if len(sys.argv) < 3:
        sys.exit("usage: render.py <template.in> <catalog.json> [key=value ...]")

    template_path, catalog_path = sys.argv[1], sys.argv[2]
    with open(catalog_path, encoding="utf-8") as fh:
        values = json.load(fh)

    for arg in sys.argv[3:]:
        if "@=" in arg and arg.index("@=") < arg.index("="):
            key, path = arg.split("@=", 1)
            with open(path, encoding="utf-8") as fh:
                values[key] = fh.read()
        elif "=" in arg:
            key, val = arg.split("=", 1)
            values[key] = val
        else:
            sys.exit("render.py: expected key=value or key@=path, got %r" % arg)

    with open(template_path, encoding="utf-8") as fh:
        text = fh.read()

    # Resolve known placeholders iteratively: an included snippet (key@=path)
    # may itself contain @@...@@ placeholders that only appear after the
    # including placeholder is substituted. Unknown placeholders are left
    # untouched during these passes.
    def repl_known(match):
        key = match.group(1)
        return values[key] if key in values else match.group(0)

    for _ in range(16):
        new = PLACEHOLDER.sub(repl_known, text)
        if new == text:
            break
        text = new

    # Final pass: any placeholder still present is genuinely unknown -> blank.
    missing = set()

    def repl_blank(match):
        missing.add(match.group(1))
        return ""

    out = PLACEHOLDER.sub(repl_blank, text)

    if missing:
        sys.stderr.write(
            "render.py: %s: unresolved placeholders blanked: %s\n"
            % (template_path, ", ".join(sorted(missing)))
        )

    sys.stdout.write(out)


if __name__ == "__main__":
    main()
