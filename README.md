# Portfolio

Personal portfolio of Antoni Aloy Torrens, built as a static, multilingual site
with [**sblg**](https://kristaps.bsd.lv/sblg/) and
[**lowdown**](https://kristaps.bsd.lv/lowdown/), driven by `make`.

Languages: **Catalan (ca)**, **Spanish (es)**, **English (en)**. Catalan is the
source language.

## Requirements

| Tool      | Used for                                  |
|-----------|-------------------------------------------|
| `sblg`    | merging article fragments into templates  |
| `lowdown` | Markdown → (XML-well-formed) HTML          |
| `jq`      | reading the JSON translation catalogs      |
| `python3` | the two build helpers in `tools/`          |
| `make`    | orchestration                              |
| `xmllint` | (optional) well-formedness checks          |

## Build

```sh
make            # build everything into public/
make build-one LANG=ca   # build a single language
make serve      # build, then serve public/ at http://127.0.0.1:8080
make clean      # remove public/
make rsync REMOTE_HOST=… REMOTE_USER=… REMOTE_PATH=…   # deploy
```

Output goes to `public/` (git-ignored). The site root `/` is **not** generated:
language selection is done by the web server (see `deploy/nginx.conf.example`).

## How it works

sblg has **no template logic** (no conditionals, no loops, no global variable
substitution) — only article/navigation listing and per-article `${sblg-*}`
symbols. So all per-language UI text and per-page values are baked into the
templates *before* sblg runs:

```
content/<lang>/*.md ──(tools/mkarticle.py + lowdown)──▶ sblg <article> fragment
templates/*.in.html + i18n/<lang>.json
                    ──(tools/render.py)──▶ concrete sblg template
fragments + concrete template ──(sblg)──▶ public/<lang>/…
```

- **`tools/mkarticle.py`** turns a Markdown file (with a small `---` front-matter
  block) into an sblg article fragment, carrying metadata as `data-sblg-*`
  attributes.
- **`tools/render.py`** substitutes `@@key@@` placeholders in a template from the
  language's JSON catalog plus per-page values (`section`, `url.self.*`, …).

Page types: home + content pages (education, projects, experience) are built in
standalone mode (`sblg -c`); the blog index, home "latest posts", and tag pages
use sblg blog mode (`<nav data-sblg-nav="1">`); each post is standalone.

### Active nav & language switcher

sblg can't branch on the current page, so the active section/language is exposed
as `<body data-section=… data-lang=…>` and the highlighting is done in CSS
(`assets/css/styles.css`). The language switcher links to the *same* page in each
language, using that language's translated slug.

## Internationalization (Weblate-compatible)

There are two distinct kinds of translatable text:

1. **UI strings** (nav labels, "Latest posts", "Tags:", footer, **and URL
   slugs**) live in `i18n/<lang>.json` — flat, dotted-key, monolingual JSON.
   - `i18n/ca.json` is the **source/base** file; `es.json` and `en.json` mirror
     its keys.
   - Edit by hand, or manage in **Weblate** with:
     - Format: **JSON file** (monolingual)
     - File mask: `i18n/*.json`
     - Monolingual base language file: `i18n/ca.json`
   - All three files must share the same key set. Check with:
     ```sh
     jq empty i18n/*.json
     diff <(jq -r 'keys[]' i18n/ca.json|sort) <(jq -r 'keys[]' i18n/en.json|sort)
     ```
   - A `.weblate` file is included for the `wlc` CLI.

2. **Page/article bodies** are *not* in Weblate. They are per-language Markdown
   files under `content/<lang>/`. If a body file is missing for a language, the
   build **falls back to Catalan** (`SRCLANG`), so every language has every page;
   add `content/es/…` or `content/en/…` to override.

### Adding a translatable UI string

1. Add the key to **all** of `i18n/ca.json`, `es.json`, `en.json`.
2. Reference it in a template as `@@your.key@@`.
3. `make`.

## Authoring content

Create a Markdown file under `content/<lang>/`:

```markdown
---
title: My Post Title
date: 2024-01-15
tags: web css
description: One-line summary used in listings and the feed.
---
Body in **Markdown**. Raw HTML is allowed and passed through verbatim
(must be XML-well-formed, e.g. self-close `<img … />` and `<br/>`).
```

- Blog posts go in `content/<lang>/blog/`; the filename (e.g.
  `2024-01-15-slug.md`) becomes the post URL.
- Content pages are `content/<lang>/{index,education,projects,experience}.md`.

> Note: tags in listings render as styled `<span>`s (sblg's `${sblg-tags}`), not
> links; the per-tag pages under `/<lang>/<blog>/tag/` are still generated and
> reachable by URL.

## Deploy

`make rsync` uploads `public/`. Configure the web server to negotiate the root
language — see `deploy/nginx.conf.example`.
