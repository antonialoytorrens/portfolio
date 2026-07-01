# Portfolio build — sblg + lowdown, with build-time i18n.
#
# sblg has no template logic, so per-language UI strings and per-page values are
# baked into the templates *before* sblg runs. There is no custom interpreter:
# article fragments are built with a small POSIX shell script (awk + lowdown),
# and templating is plain sed (@@key@@ substitutions generated from the JSON
# catalog with jq, via the subst() shell function):
#
#   content/<lang>/*.md  --(tools/mkarticle.sh: awk + lowdown)-->  sblg fragment
#   partials/head + templates/<page>.html + partials/foot
#                        --(assemble | inject | subst)-->  concrete template
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
MKART   := sh tools/mkarticle.sh

HEAD_TPL  := $(TEMPLATE_DIR)/partials/head.html
FOOT_TPL  := $(TEMPLATE_DIR)/partials/foot.html
ITEM_TPL  := $(TEMPLATE_DIR)/partials/postitem.html

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
	cat assets/css/reset.css assets/css/styles.css > $(PUBLIC_DIR)/assets/css/bundle.css

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
	# flatsed [key=value...] : emit a sed script of flat @@key@@ -> value
	# substitutions, drawn from the JSON catalog ($J) plus any extra key=value
	# pairs. SOH (0x01) is used as the sed delimiter so values may contain
	# / | $ { } etc. freely; only \ and & (sed-special on the RHS) are escaped,
	# and '.' in keys is escaped so it matches literally.
	flatsed() {
	  local D TAB k v kv
	  D=$$(printf '\001'); TAB=$$(printf '\t')
	  { $(JQ) -r 'to_entries[]|"\(.key)\t\(.value)"' "$$J"
	    for kv in "$$@"; do printf '%s\t%s\n' "$${kv%%=*}" "$${kv#*=}"; done
	  } | while IFS=$$TAB read -r k v; do
	    k=$${k//./\\.}
	    v=$${v//\\/\\\\}; v=$${v//&/\\&}
	    printf 's%s@@%s@@%s%s%sg\n' "$$D" "$$k" "$$D" "$$v" "$$D"
	  done
	}
	# subst <template|-> [key=value...] : flat @@key@@ substitution -> stdout. Use - for stdin.
	subst() {
	  local t=$$1; shift
	  [ "$$t" = - ] && t=/dev/stdin
	  sed -f <(flatsed lang="$$L" author="$(AUTHOR)" year="$(YEAR)" "$$@") "$$t"
	}
	# assemble <name> : cat partials/head + templates/<name>.html + partials/foot -> stdout.
	assemble() { cat $(HEAD_TPL) "$(TEMPLATE_DIR)/$$1.html" $(FOOT_TPL); }
	# inject [KEY=FILE ...] : replace each @@KEY@@ placeholder with the contents of FILE.
	inject() {
	  sed -f <(for arg in "$$@"; do
	    printf '/@@%s@@/{\nr %s\nd\n}\n' "$${arg%%=*}" "$${arg#*=}"
	  done)
	}
	# link_tags <file> : sblg renders ${sblg-tags} as <span class="sblg-tag"> with no href; convert to <a>.
	link_tags() {
	  sed -i "s|<span class=\"sblg-tag\">\([^<]*\)</span>|<a class=\"sblg-tag\" href=\"/$$L/$$SB/tag/\1.html\">\1</a>|g" "$$1"
	}

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
	assemble index \
	  | inject home_intro=$(WORK_DIR)/$$L/intro.html post_item=$(ITEM_TPL) \
	  | subst - section=about page_heading="$$TH" page_title="$$TH" page_description="" $$sw_home \
	  > $(WORK_DIR)/$$L/tmpl-index.html
	$(SBLG) -o $(PUBLIC_DIR)/$$L/index.html -t $(WORK_DIR)/$$L/tmpl-index.html "$${FRAGS[@]}"
	link_tags $(PUBLIC_DIR)/$$L/index.html

	# --- standalone content pages (-c) -----------------------------------
	build_page() { # <section> <name> <slug> <switcher>
	  local sec=$$1 name=$$2 slug=$$3 sw=$$4
	  mkdir -p $(PUBLIC_DIR)/$$L/$$slug
	  $(MKART) "$$(pagesrc $$name)" -o $(WORK_DIR)/$$L/page-$$name.xml
	  assemble page \
	    | subst - \
	      section=$$sec page_heading='$${sblg-title}' page_title='$${sblg-titletext}' \
	      page_description='$${sblg-aside}' $$sw \
	    > $(WORK_DIR)/$$L/tmpl-$$name.html
	  $(SBLG) -c -o $(PUBLIC_DIR)/$$L/$$slug/index.html \
	    -t $(WORK_DIR)/$$L/tmpl-$$name.html $(WORK_DIR)/$$L/page-$$name.xml
	}
	build_page education education "$$SE" "$$sw_edu"
	build_page projects   projects   "$$SP" "$$sw_proj"
	build_page experience experience "$$SX" "$$sw_exp"

	# --- blog index (/<lang>/<blog>/) ------------------------------------
	mkdir -p $(PUBLIC_DIR)/$$L/$$SB
	TB=$$(val title.blog)
	assemble blog \
	  | inject post_item=$(ITEM_TPL) \
	  | subst - section=blog page_heading="$$TB" page_title="$$TB" page_description="" $$sw_blog \
	  > $(WORK_DIR)/$$L/tmpl-blog.html
	$(SBLG) -o $(PUBLIC_DIR)/$$L/$$SB/index.html -t $(WORK_DIR)/$$L/tmpl-blog.html "$${FRAGS[@]}"
	link_tags $(PUBLIC_DIR)/$$L/$$SB/index.html

	# --- individual posts (-c) -------------------------------------------
	mkdir -p $(PUBLIC_DIR)/$$L/$$SB/post
	assemble post \
	  | subst - \
	    section=blog page_heading='$${sblg-title}' page_title='$${sblg-titletext}' \
	    page_description='$${sblg-aside}' $$sw_blog \
	  > $(WORK_DIR)/$$L/tmpl-post.html
	for fr in "$${FRAGS[@]}"; do
	  n=$$(basename $$fr .xml)
	  $(SBLG) -c -o $(PUBLIC_DIR)/$$L/$$SB/post/$$n.html -t $(WORK_DIR)/$$L/tmpl-post.html $$fr
	  link_tags $(PUBLIC_DIR)/$$L/$$SB/post/$$n.html
	done

	# --- tag pages -------------------------------------------------------
	mkdir -p $(PUBLIC_DIR)/$$L/$$SB/tag
	THEAD=$$(val tag.heading)
	for t in $$($(SBLG) -l "$${FRAGS[@]}" | cut -f2 | sort -u); do
	  [ -n "$$t" ] || continue
	  assemble tag \
	    | inject post_item=$(ITEM_TPL) \
	    | subst - \
	      section=blog tag="$$t" page_heading="$$THEAD $$t" page_title="$$THEAD $$t" page_description="" $$sw_blog \
	    > $(WORK_DIR)/$$L/tmpl-tag-$$t.html
	  $(SBLG) -o $(PUBLIC_DIR)/$$L/$$SB/tag/$$t.html -t $(WORK_DIR)/$$L/tmpl-tag-$$t.html "$${FRAGS[@]}"
	  link_tags $(PUBLIC_DIR)/$$L/$$SB/tag/$$t.html
	done

	# --- Atom feed (/<lang>/atom.xml) ------------------------------------
	subst $(TEMPLATE_DIR)/atom.in.xml base_url="$(BASE_DOMAIN)" > $(WORK_DIR)/$$L/atom-tmpl.xml
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
