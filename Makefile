# Portfolio build — sblg + lowdown, with build-time i18n.
#
# sblg has no template logic, so per-language UI strings and per-page values are
# baked into the templates *before* sblg runs:
#
#   content/<lang>/*.md  --(tools/mkarticle.py + lowdown)-->  sblg article fragment
#   templates/_base.in.html + main-*.html + i18n/<lang>.json
#                        --(tools/render.py)-->  concrete sblg template
#   fragments + concrete template  --(sblg)-->  public/<lang>/...
#
# Body content that is not yet translated falls back to the source language
# (SRCLANG), so every language has every page; the chrome is always localised.

SHELL        := /bin/bash
.ONESHELL:
.SHELLFLAGS  := -ec

# ---- Configuration -------------------------------------------------------
AUTHOR       := Antoni Aloy Torrens
BASE_DOMAIN  := https://antonialoytorrens.com
LANGS        := ca es en
SRCLANG      := ca
YEAR         := $(shell date +%Y)

CONTENT_DIR  := content
I18N_DIR     := i18n
TEMPLATE_DIR := templates
PUBLIC_DIR   := public
WORK_DIR     := $(PUBLIC_DIR)/.work

# ---- Tools ---------------------------------------------------------------
SBLG    := sblg
LOWDOWN := lowdown --html-no-skiphtml --html-no-escapehtml
JQ      := jq
RENDER  := python3 tools/render.py
MKART   := python3 tools/mkarticle.py

BASE_TPL := $(TEMPLATE_DIR)/_base.in.html
ITEM_TPL := $(TEMPLATE_DIR)/_postitem.html

# Local preview server
SERVE_HOST ?= 127.0.0.1
SERVE_PORT ?= 8080

# Deploy (rsync)
REMOTE_USER ?= user
REMOTE_HOST ?= example.com
REMOTE_PATH ?= /var/www/html
RSYNC_OPTS  ?= -avz --delete

.PHONY: all assets clean serve rsync help build-one

all: assets
	@for l in $(LANGS); do $(MAKE) --no-print-directory build-one LANG=$$l; done
	@echo "Build complete -> $(PUBLIC_DIR)/"

assets:
	@echo "==> Copying assets"
	mkdir -p $(PUBLIC_DIR)
	cp -a assets $(PUBLIC_DIR)/

# Build everything for one language. Pass LANG=<lang>.
build-one:
	@L=$(LANG)
	J=$(I18N_DIR)/$$L.json
	echo "==> Building language '$$L'"

	# --- helpers ---------------------------------------------------------
	val() { $(JQ) -r --arg k "$$1" '.[$$k]' "$$J"; }
	slug() { $(JQ) -r --arg k "slug.$$2" '.[$$k]' $(I18N_DIR)/$$1.json; }
	# Prefer the language's own content file, else fall back to SRCLANG.
	pagesrc() {
	  if [ -f $(CONTENT_DIR)/$$L/$$1.md ]; then echo $(CONTENT_DIR)/$$L/$$1.md
	  else echo $(CONTENT_DIR)/$(SRCLANG)/$$1.md; fi
	}
	# render <out> <extra render args...> : base template -> concrete template
	render() { local out=$$1; shift; $(RENDER) $(BASE_TPL) "$$J" \
	    lang="$$L" author="$(AUTHOR)" year="$(YEAR)" "$$@" > "$$out"; }

	# --- slugs (this language + every language, for the switcher) --------
	SB=$$(val slug.blog); SE=$$(val slug.education)
	SP=$$(val slug.projects); SX=$$(val slug.experience)
	declare -A BLOG EDU PROJ EXP
	for x in $(LANGS); do
	  BLOG[$$x]=$$(slug $$x blog);       EDU[$$x]=$$(slug $$x education)
	  PROJ[$$x]=$$(slug $$x projects);   EXP[$$x]=$$(slug $$x experience)
	done
	sw_home="url.self.ca=/ca/ url.self.es=/es/ url.self.en=/en/"
	sw_blog="url.self.ca=/ca/$${BLOG[ca]}/ url.self.es=/es/$${BLOG[es]}/ url.self.en=/en/$${BLOG[en]}/"
	sw_edu="url.self.ca=/ca/$${EDU[ca]}/ url.self.es=/es/$${EDU[es]}/ url.self.en=/en/$${EDU[en]}/"
	sw_proj="url.self.ca=/ca/$${PROJ[ca]}/ url.self.es=/es/$${PROJ[es]}/ url.self.en=/en/$${PROJ[en]}/"
	sw_exp="url.self.ca=/ca/$${EXP[ca]}/ url.self.es=/es/$${EXP[es]}/ url.self.en=/en/$${EXP[en]}/"

	# --- blog post fragments (with fallback) -----------------------------
	mkdir -p $(WORK_DIR)/$$L/blog $(PUBLIC_DIR)/$$L
	BLOGSRC=$(CONTENT_DIR)/$$L/blog
	ls $$BLOGSRC/*.md >/dev/null 2>&1 || BLOGSRC=$(CONTENT_DIR)/$(SRCLANG)/blog
	FRAGS=()
	if ls $$BLOGSRC/*.md >/dev/null 2>&1; then
	  for f in $$BLOGSRC/*.md; do
	    n=$$(basename $$f .md)
	    $(MKART) $$f --source "$$SB/post/$$n.html" -o $(WORK_DIR)/$$L/blog/$$n.xml
	    FRAGS+=($(WORK_DIR)/$$L/blog/$$n.xml)
	  done
	fi

	# --- home page (/<lang>/) : intro + latest posts (blog mode) ---------
	$(LOWDOWN) "$$(pagesrc index)" > $(WORK_DIR)/$$L/intro.html
	TH=$$(val title.home)
	render $(WORK_DIR)/$$L/tmpl-index.html section=about \
	  page_heading="$$TH" page_title="$$TH" page_description="" \
	  main@=$(TEMPLATE_DIR)/main-index.html post_item@=$(ITEM_TPL) \
	  home_intro@=$(WORK_DIR)/$$L/intro.html $$sw_home
	$(SBLG) -o $(PUBLIC_DIR)/$$L/index.html -t $(WORK_DIR)/$$L/tmpl-index.html "$${FRAGS[@]}"

	# --- standalone content pages (-c) -----------------------------------
	build_page() { # <section> <name> <slug> <switcher>
	  local sec=$$1 name=$$2 slug=$$3 sw=$$4
	  mkdir -p $(PUBLIC_DIR)/$$L/$$slug
	  $(MKART) "$$(pagesrc $$name)" -o $(WORK_DIR)/$$L/page-$$name.xml
	  render $(WORK_DIR)/$$L/tmpl-$$name.html section=$$sec \
	    page_heading='$${sblg-title}' page_title='$${sblg-titletext}' \
	    page_description='$${sblg-get|description}' \
	    main@=$(TEMPLATE_DIR)/main-page.html $$sw
	  $(SBLG) -c -o $(PUBLIC_DIR)/$$L/$$slug/index.html \
	    -t $(WORK_DIR)/$$L/tmpl-$$name.html $(WORK_DIR)/$$L/page-$$name.xml
	}
	build_page education education "$$SE" "$$sw_edu"
	build_page projects   projects   "$$SP" "$$sw_proj"
	build_page experience experience "$$SX" "$$sw_exp"

	# --- blog index (/<lang>/<blog>/) ------------------------------------
	mkdir -p $(PUBLIC_DIR)/$$L/$$SB
	TB=$$(val title.blog)
	render $(WORK_DIR)/$$L/tmpl-blog.html section=blog \
	  page_heading="$$TB" page_title="$$TB" page_description="" \
	  main@=$(TEMPLATE_DIR)/main-blog.html post_item@=$(ITEM_TPL) $$sw_blog
	$(SBLG) -o $(PUBLIC_DIR)/$$L/$$SB/index.html -t $(WORK_DIR)/$$L/tmpl-blog.html "$${FRAGS[@]}"

	# --- individual posts (-c) -------------------------------------------
	mkdir -p $(PUBLIC_DIR)/$$L/$$SB/post
	for fr in "$${FRAGS[@]}"; do
	  n=$$(basename $$fr .xml)
	  render $(WORK_DIR)/$$L/tmpl-post-$$n.html section=blog \
	    page_heading='$${sblg-title}' page_title='$${sblg-titletext}' \
	    page_description='$${sblg-get|description}' \
	    main@=$(TEMPLATE_DIR)/main-post.html $$sw_blog
	  $(SBLG) -c -o $(PUBLIC_DIR)/$$L/$$SB/post/$$n.html \
	    -t $(WORK_DIR)/$$L/tmpl-post-$$n.html $$fr
	done

	# --- tag pages -------------------------------------------------------
	mkdir -p $(PUBLIC_DIR)/$$L/$$SB/tag
	THEAD=$$(val tag.heading)
	for t in $$($(SBLG) -l "$${FRAGS[@]}" | cut -f2 | sort -u); do
	  [ -n "$$t" ] || continue
	  render $(WORK_DIR)/$$L/tmpl-tag-$$t.html section=blog tag="$$t" \
	    page_heading="$$THEAD $$t" page_title="$$THEAD $$t" page_description="" \
	    main@=$(TEMPLATE_DIR)/main-tag.html post_item@=$(ITEM_TPL) $$sw_blog
	  $(SBLG) -o $(PUBLIC_DIR)/$$L/$$SB/tag/$$t.html \
	    -t $(WORK_DIR)/$$L/tmpl-tag-$$t.html "$${FRAGS[@]}"
	done

	# --- Atom feed (/<lang>/atom.xml) ------------------------------------
	$(RENDER) $(TEMPLATE_DIR)/atom.in.xml "$$J" \
	  lang="$$L" author="$(AUTHOR)" base_url="$(BASE_DOMAIN)" \
	  > $(WORK_DIR)/$$L/atom-tmpl.xml
	$(SBLG) -a -o $(PUBLIC_DIR)/$$L/atom.xml -t $(WORK_DIR)/$$L/atom-tmpl.xml "$${FRAGS[@]}"

	echo "    language '$$L' done."

clean:
	@echo "==> Cleaning $(PUBLIC_DIR)/"
	rm -rf $(PUBLIC_DIR)

serve: all
	@echo "==> Serving $(PUBLIC_DIR)/ at http://$(SERVE_HOST):$(SERVE_PORT)/"
	cd $(PUBLIC_DIR) && python3 -m http.server $(SERVE_PORT) --bind $(SERVE_HOST)

rsync: all
	@echo "==> Syncing $(PUBLIC_DIR)/ to $(REMOTE_USER)@$(REMOTE_HOST):$(REMOTE_PATH)"
	rsync $(RSYNC_OPTS) $(PUBLIC_DIR)/ $(REMOTE_USER)@$(REMOTE_HOST):$(REMOTE_PATH)

help:
	@echo "Targets: all (default), assets, build-<lang>, clean, serve, rsync"
	@echo "Languages: $(LANGS)  (source/fallback: $(SRCLANG))"
