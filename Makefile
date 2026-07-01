#!/usr/bin/make -f
SHELL        := /bin/bash
.ONESHELL:
.SHELLFLAGS  := -ec

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

SBLG    := sblg
LOWDOWN := lowdown --html-no-skiphtml --html-no-escapehtml
JQ      := jq

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

LANG_TARGETS := $(addprefix build-,$(LANGS))

.PHONY: all assets clean serve rsync help build-one $(LANG_TARGETS)

all: $(LANG_TARGETS)
	@echo "Build complete -> $(PUBLIC_DIR)/"

# Per-language targets
$(LANG_TARGETS): build-%: assets
	@$(MAKE) --no-print-directory build-one LANG=$*

assets:
	@echo "==> Copying assets"
	mkdir -p $(PUBLIC_DIR)
	cp -a assets $(PUBLIC_DIR)/
	cat assets/css/reset.css assets/css/styles.css > $(PUBLIC_DIR)/assets/css/bundle.css

# Build for one language (pass LANG=<lang>)
build-one:
	@L=$(LANG)
	J=$(I18N_DIR)/$$L.json
	echo "==> Building language '$$L'"

	# Helpers
	val() { $(JQ) -r --arg k "$$1" '.[$$k]' "$$J"; }
	slug() { $(JQ) -r --arg k "slug.$$2" '.[$$k]' $(I18N_DIR)/$$1.json; }
	# Fall back to SRCLANG if content unavailable
	pagesrc() {
	  if [ -f $(CONTENT_DIR)/$$L/$$1.md ]; then echo $(CONTENT_DIR)/$$L/$$1.md
	  else echo $(CONTENT_DIR)/$(SRCLANG)/$$1.md; fi
	}
	# Generate language URLs for the switcher
	switcher() {
	  for x in $(LANGS); do
	    if [ -n "$$1" ]; then printf 'url.self.%s=/%s/%s/ ' "$$x" "$$x" "$$(slug $$x $$1)"
	    else printf 'url.self.%s=/%s/ ' "$$x" "$$x"; fi
	  done
	}
	# Generate sed script for @@key@@ substitutions from JSON + key=value pairs
	flatsed() {
	  $(JQ) -rn --args '
	    ( input | to_entries ) +
	    ( $$ARGS.positional | map(index("=") as $$i | {key: .[:$$i], value: .[$$i+1:]}) )
	    | .[]
	    | ( .key            | gsub("\\.";  "\\.") )                    as $$k
	    | ( .value|tostring | gsub("\\\\"; "\\\\") | gsub("&"; "\\&") ) as $$v
	    | "s\u0001@@\($$k)@@\u0001\($$v)\u0001g"
	  ' "$$@" < "$$J"
	}
	# Apply @@key@@ substitutions (- for stdin)
	subst() {
	  local t=$$1; shift
	  [ "$$t" = - ] && t=/dev/stdin
	  sed -f <(flatsed lang="$$L" author="$(AUTHOR)" year="$(YEAR)" "$$@") "$$t"
	}
	# Concatenate head + template + foot
	assemble() { cat $(HEAD_TPL) "$(TEMPLATE_DIR)/$$1.html" $(FOOT_TPL); }
	# Replace @@KEY@@ placeholders with file contents
	inject() {
	  sed -f <(for arg in "$$@"; do
	    printf '/@@%s@@/{\nr %s\nd\n}\n' "$${arg%%=*}" "$${arg#*=}"
	  done)
	}
	# Markdown + front matter -> sblg <article> fragment (mkart <md> <out> [source]).
	# Maps every front-matter key to its sblg field; unknown keys -> data-sblg-set-*.
	xmlesc() { sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g; s/"/\&quot;/g'; }
	mkart() {
	  local md=$$1 out=$$2 source=$${3:-} key val desc=""
	  {
	    printf '<article data-sblg-article="1"'
	    [ -n "$$source" ] && printf ' data-sblg-source="%s"' "$$(printf %s "$$source" | xmlesc)"
	    while read -r key; do
	      [ -n "$$key" ] || continue
	      val=$$($(LOWDOWN) -X "$$key" "$$md")
	      case "$$key" in
	        title)       printf ' data-sblg-title="%s"'    "$$(printf %s "$$val"        | xmlesc)" ;;
	        date)        printf ' data-sblg-datetime="%s"' "$$(printf %s "$${val:0:10}" | xmlesc)" ;;
	        tags)        printf ' data-sblg-tags="%s"'     "$$(printf %s "$$val"        | xmlesc)" ;;
	        img)         printf ' data-sblg-img="%s"'      "$$(printf %s "$$val"        | xmlesc)" ;;
	        author)      printf ' data-sblg-author="%s"'   "$$(printf %s "$$val"        | xmlesc)" ;;
	        description) desc=$$val ;;
	        *)           printf ' data-sblg-set-%s="%s"' "$$key" "$$(printf %s "$$val" | xmlesc)" ;;
	      esac
	    done < <($(LOWDOWN) -L "$$md")
	    printf '>\n'
	    [ -n "$$desc" ] && printf '<aside>%s</aside>\n' "$$(printf %s "$$desc" | xmlesc)"
	    $(LOWDOWN) "$$md"
	    printf '</article>\n'
	  } > "$$out"
	}
	# Convert sblg spans to tag links
	link_tags() {
	  sed -i "s|<span class=\"sblg-tag\">\([^<]*\)</span>|<a class=\"sblg-tag\" href=\"/$$L/$$SB/tag/\1.html\">\1</a>|g" "$$1"
	}

	# Slugs & language switchers
	SB=$$(val slug.blog)
	sw_home=$$(switcher); sw_blog=$$(switcher blog)

	# Blog post fragments (fall back to SRCLANG if not found)
	mkdir -p $(WORK_DIR)/$$L/blog $(PUBLIC_DIR)/$$L
	BLOGSRC=$(CONTENT_DIR)/$$L/blog
	ls $$BLOGSRC/*.md >/dev/null 2>&1 || BLOGSRC=$(CONTENT_DIR)/$(SRCLANG)/blog
	FRAGS=()
	if ls $$BLOGSRC/*.md >/dev/null 2>&1; then
	  for f in $$BLOGSRC/*.md; do
	    n=$$(basename $$f .md)
	    mkart "$$f" $(WORK_DIR)/$$L/blog/$$n.xml "$$SB/post/$$n.html"
	    FRAGS+=($(WORK_DIR)/$$L/blog/$$n.xml)
	  done
	fi

	# Home page: intro + latest posts
	$(LOWDOWN) "$$(pagesrc index)" > $(WORK_DIR)/$$L/intro.html
	TH=$$(val title.home)
	assemble index \
	  | inject home_intro=$(WORK_DIR)/$$L/intro.html post_item=$(ITEM_TPL) \
	  | subst - section=about page_heading="$$TH" page_title="$$TH" page_description="" $$sw_home \
	  > $(WORK_DIR)/$$L/tmpl-index.html
	$(SBLG) -o $(PUBLIC_DIR)/$$L/index.html -t $(WORK_DIR)/$$L/tmpl-index.html "$${FRAGS[@]}"
	link_tags $(PUBLIC_DIR)/$$L/index.html

	# Standalone content pages
	build_page() {
	  local name=$$1 slug
	  slug=$$(val slug.$$name)
	  mkdir -p $(PUBLIC_DIR)/$$L/$$slug
	  mkart "$$(pagesrc $$name)" $(WORK_DIR)/$$L/page-$$name.xml
	  assemble page \
	    | subst - \
	      section=$$name page_heading='$${sblg-title}' page_title='$${sblg-titletext}' \
	      page_description='$${sblg-aside}' $$(switcher $$name) \
	    > $(WORK_DIR)/$$L/tmpl-$$name.html
	  $(SBLG) -c -o $(PUBLIC_DIR)/$$L/$$slug/index.html \
	    -t $(WORK_DIR)/$$L/tmpl-$$name.html $(WORK_DIR)/$$L/page-$$name.xml
	}
	build_page education
	build_page projects
	build_page experience

	# Blog index
	mkdir -p $(PUBLIC_DIR)/$$L/$$SB
	TB=$$(val title.blog)
	assemble blog \
	  | inject post_item=$(ITEM_TPL) \
	  | subst - section=blog page_heading="$$TB" page_title="$$TB" page_description="" $$sw_blog \
	  > $(WORK_DIR)/$$L/tmpl-blog.html
	$(SBLG) -o $(PUBLIC_DIR)/$$L/$$SB/index.html -t $(WORK_DIR)/$$L/tmpl-blog.html "$${FRAGS[@]}"
	link_tags $(PUBLIC_DIR)/$$L/$$SB/index.html

	# Individual posts
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

	# Tag pages
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

	# Atom feed
	subst $(TEMPLATE_DIR)/atom.in.xml base_url="$(BASE_DOMAIN)" > $(WORK_DIR)/$$L/atom-tmpl.xml
	$(SBLG) -a -o $(PUBLIC_DIR)/$$L/atom.xml -t $(WORK_DIR)/$$L/atom-tmpl.xml "$${FRAGS[@]}"

	echo "    language '$$L' done."

clean:
	@echo "==> Cleaning $(PUBLIC_DIR)/"
	rm -rf $(PUBLIC_DIR)

serve:
	@echo "==> Serving $(PUBLIC_DIR)/ at http://$(SERVE_HOST):$(SERVE_PORT)/"
	cd $(PUBLIC_DIR) && python3 -m http.server $(SERVE_PORT) --bind $(SERVE_HOST)

rsync:
	@echo "==> Syncing $(PUBLIC_DIR)/ to $(REMOTE_USER)@$(REMOTE_HOST):$(REMOTE_PATH)"
	rsync $(RSYNC_OPTS) $(PUBLIC_DIR)/ $(REMOTE_USER)@$(REMOTE_HOST):$(REMOTE_PATH)

help:
	@echo "Targets: all (default), assets, build-<lang>, clean, serve, rsync"
	@echo "Languages: $(LANGS)  (source/fallback: $(SRCLANG))"
	@echo "Parallel: 'make -j' builds languages concurrently (add -Otarget to group output)"
