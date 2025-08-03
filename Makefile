# Project variables
SHELL := /bin/sh
AUTHOR_NAME = "Antoni Aloy Torrens"
AUTHOR_EMAIL = "author@example.org"
SITE_TITLE = "Portfolio"
SITE_TAGLINE = "Site Tagline"
BASE_DOMAIN = "http://example.org"
BASE_URL = "http://127.0.0.1:8080"
DATE_FORMAT = "%b %d, %Y, %I:%M %p GMT"

# Directories
CONTENT_DIR = content
PUBLIC_DIR = public
TEMPLATE_DIR = templates

# Languages
LANGS := ca en es

# Include translation mappings
include translations.mk

# Templates
MAIN_TPL = $(TEMPLATE_DIR)/index.tmpl
POST_TPL = $(TEMPLATE_DIR)/post.tmpl
TAG_TPL = $(TEMPLATE_DIR)/tag.tmpl

# Blog configuration
LATEST_POSTS_COUNT = 5
POSTS_PER_PAGE = 10

# Base blogc command with common variables
BLOGC_BASE = \
	$(BLOGC) \
	-D AUTHOR_NAME=$(AUTHOR_NAME) \
	-D AUTHOR_EMAIL=$(AUTHOR_EMAIL) \
	-D SITE_TITLE=$(SITE_TITLE) \
	-D SITE_TAGLINE=$(SITE_TAGLINE) \
	-D BASE_DOMAIN=$(BASE_DOMAIN) \
	-D BASE_URL=$(BASE_URL) \
	-D DATE_FORMAT=$(DATE_FORMAT)

# Blogc local server configuration
BLOGC_RUNSERVER_HOST ?= 127.0.0.1
BLOGC_RUNSERVER_PORT ?= 8080

# Rsync configuration
REMOTE_USER ?= user
REMOTE_HOST ?= example.com
REMOTE_PATH ?= /var/www/html
RSYNC_OPTIONS ?= -avz --delete

# Shell commands
BLOGC ?= $(shell which blogc)
BLOGC_RUNSERVER ?= $(shell which blogc-runserver)
RSYNC ?= $(shell which rsync)
MKDIR ?= $(shell which mkdir)
CP ?= $(shell which cp)


# TODO: see https://stackoverflow.com/questions/16144115/makefile-remove-duplicate-words-without-sorting
SORT ?= $(shell which sort)
UNIQ ?= $(shell which uniq)

# Build targets
.PHONY: all assets clean serve rsync lang-pages lang-blog lang-tags lang-index

all: assets $(addprefix build-, $(LANGS))

assets:
	@echo "Copying assets..."
	@$(MKDIR) -p $(PUBLIC_DIR)
	@$(CP) -ar assets $(PUBLIC_DIR)

# Language-specific build targets
build-%:
	@echo "Building language '$*'..."
	@$(MAKE) lang-pages LANG=$*
	@$(MAKE) lang-blog LANG=$*
	@$(MAKE) lang-tags LANG=$*
	@$(MAKE) lang-index LANG=$*
	@echo "Language $* built successfully."

# Build individual pages for a language (excluding index, which is handled separately)
lang-pages:
	@echo " - Building pages for $(LANG)..."
	@$(foreach page,$(filter-out $(CONTENT_DIR)/$(LANG)/index.txt,$(wildcard $(CONTENT_DIR)/$(LANG)/*.txt)),\
		page_name=$(basename $(notdir $(page))); \
		translation_vars="$(call get_page_translations,$(basename $(notdir $(page))),$(LANG))"; \
		if [ -n "$$translation_vars" ]; then \
			echo "   Building page: $$page_name"; \
			echo "	 Translation vars: $$translation_vars" \
			$(MKDIR) -p $(PUBLIC_DIR)/$(LANG)/$$page_name; \
			$(BLOGC_BASE) \
				-D MENU=$$page_name \
				-D LANG=$(LANG) \
				-D SETLANG=/$(LANG)/$$page_name \
				$$translation_vars \
				-o $(PUBLIC_DIR)/$(LANG)/$$page_name/index.html \
				-t $(MAIN_TPL) \
				$(page); \
		else \
			echo "   Skipping page: $$page_name (no translation mapping found)"; \
		fi; \
	)

# Build index page with latest posts
lang-index:
	@echo " - Building index with latest posts for $(LANG)..."
	@if [ -f "$(CONTENT_DIR)/$(LANG)/index.txt" ]; then \
		blog_posts=$$(find $(CONTENT_DIR)/$(LANG)/blog -name "*.txt" 2>/dev/null | head -$(LATEST_POSTS_COUNT) || true); \
		translation_vars="$(call get_page_translations,index,$(LANG))"; \
		echo "   Building index with latest posts"; \
		echo "   Blog posts: $$blog_posts"; \
		echo "	 Translation vars: $$translation_vars"; \
		if [ -n "$$blog_posts" ]; then \
			$(BLOGC_BASE) \
				-l -e $(CONTENT_DIR)/$(LANG)/index.txt \
				-D MENU=index \
				-D LANG=$(LANG) \
				-D FILTER_SORT=1 \
				-D FILTER_PER_PAGE=$(LATEST_POSTS_COUNT) \
				-D FILTER_PAGE=1 \
				$$translation_vars \
				-o $(PUBLIC_DIR)/$(LANG)/index.html \
				-t $(MAIN_TPL) \
				$$blog_posts; \
		else \
			$(BLOGC_BASE) \
				-D MENU=index \
				-D LANG=$(LANG) \
				$$translation_vars \
				-o $(PUBLIC_DIR)/$(LANG)/index.html \
				-t $(MAIN_TPL) \
				$(CONTENT_DIR)/$(LANG)/index.txt; \
		fi; \
	fi

# Build blog for a language
lang-blog:
	@echo " - Building blog for $(LANG)..."
	@$(foreach post,$(wildcard $(CONTENT_DIR)/$(LANG)/blog/*.txt),\
		post_id=$(basename $(notdir $(post))); \
		blog_vars="$(call get_page_translations,blog,$(LANG))"; \
		echo "   Building post: $$post_id"; \
		$(BLOGC_BASE) \
			-D MENU=blog \
			-D LANG=$(LANG) \
			-D IS_POST=1 \
			$$blog_vars \
			-o $(PUBLIC_DIR)/$(LANG)/blog/post/$$post_id.html \
			-t $(POST_TPL) \
			$(post); \
	)
	
	# Build blog index (all posts)
	@if [ -n "$$(find $(CONTENT_DIR)/$(LANG)/blog -name "*.txt" 2>/dev/null)" ]; then \
		blog_vars="$(call get_page_translations,blog,$(LANG))"; \
		echo "   Building blog index for $(LANG)"; \
		$(BLOGC_BASE) \
			-l \
			-D MENU=blog \
			-D LANG=$(LANG) \
			-D FILTER_SORT=1 \
			-D FILTER_PER_PAGE=$(POSTS_PER_PAGE) \
			-D FILTER_PAGE=1 \
			$$blog_vars \
			-o $(PUBLIC_DIR)/$(LANG)/blog/index.html \
			-t $(MAIN_TPL) \
			$(wildcard $(CONTENT_DIR)/$(LANG)/blog/*.txt); \
	fi

# Build tag pages for a language
lang-tags:
	@echo " - Building tag pages for $(LANG)…" && \
	tags=$$(awk '/^TAGS:/ { for(i=2; i<=NF; i++) print $$i }' \
	    $(CONTENT_DIR)/$(LANG)/blog/*.txt 2>/dev/null | sort -u) && \
	for tag in $$tags; do \
	  [ -n "$$tag" ] || continue; \
	  echo "   → Building tag page: $$tag"; \
	  blog_vars="$(call get_page_translations,blog,$(LANG))"; \
	  $(BLOGC_BASE) \
	    -l \
	    -D MENU=blog \
	    -D LANG=$(LANG) \
	    -D FILTER_TAG="$$tag" \
	    -D FILTER_SORT=1 \
	    -D CURRENT_TAG="$$tag" \
	    $$blog_vars \
	    -o $(PUBLIC_DIR)/$(LANG)/tag/$$tag.html \
	    -t $(TAG_TPL) \
	    $(CONTENT_DIR)/$(LANG)/blog/*.txt; \
	done

clean:
	@echo "Cleaning up..."
	@echo "Deleting $(PUBLIC_DIR) directory"
	@rm -rf $(PUBLIC_DIR)

# Serve the site locally
ifneq ($(BLOGC_RUNSERVER),)
serve:
	@echo "Starting server at http://$(BLOGC_RUNSERVER_HOST):$(BLOGC_RUNSERVER_PORT)"
	$(BLOGC_RUNSERVER) \
		-t $(BLOGC_RUNSERVER_HOST) \
		-p $(BLOGC_RUNSERVER_PORT) \
		$(PUBLIC_DIR)
else
serve:
	@echo "Error: blogc-runserver not found. Please install blogc with server support."
	@exit 1
endif

# Deploy to remote server via rsync
ifneq ($(RSYNC),)
rsync: all
	@echo "Syncing $(PUBLIC_DIR)/ to $(REMOTE_USER)@$(REMOTE_HOST):$(REMOTE_PATH)"
	$(RSYNC) $(RSYNC_OPTIONS) $(PUBLIC_DIR)/ $(REMOTE_USER)@$(REMOTE_HOST):$(REMOTE_PATH)
	@echo "-> Sync completed"
else
rsync:
	@echo "Error: rsync not found. Please install rsync."
	@exit 1
endif

# Debug target to show translation variables for a page
debug-translations:
	@echo "Available languages: $(LANGS)"
	@echo "Active languages: $(LANGS)"
	@echo ""
	@echo "Example for 'educacio' in Catalan:"
	@echo "$(call get_page_translations,educacio,ca)"
	@echo ""
	@echo "Example for 'education' in English:"
	@echo "$(call get_page_translations,education,en)"

# Helper target to list all tags for a language
list-tags-%:
	@echo "Tags for language '$*':"
	@{ \
	  $(foreach post,$(wildcard $(CONTENT_DIR)/$*/blog/*.txt), \
	    grep "^TAGS:" $(post) | sed 's/^TAGS: *//' | tr ' ' '\n'; \
	  ) \
	} | $(SORT) | $(UNIQ) | grep -v '^$$'


# Default target
.DEFAULT_GOAL := all