# Project variables
SHELL := /bin/sh
AUTHOR_NAME = "Author"
AUTHOR_EMAIL = "author@example.org"
SITE_TITLE = "Site Title"
SITE_TAGLINE = "Site Tagline"
BASE_DOMAIN = "http://example.org"
BASE_URL = ""
DATE_FORMAT = "%b %d, %Y, %I:%M %p GMT"

# Directories
CONTENT_DIR = content
PUBLIC_DIR = public
TEMPLATE_DIR = templates

# Include translation mappings
include translations.mk

# Languages (can override ALL_LANGS from mappings.mk if needed)
LANGS ?= $(filter ca en es, $(ALL_LANGS))

# Templates
MAIN_TPL = $(TEMPLATE_DIR)/index.tmpl
INDEX_TPL = $(TEMPLATE_DIR)/index.tmpl
POST_TPL = $(TEMPLATE_DIR)/index.tmpl

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

# Build targets
.PHONY: all assets clean serve rsync lang-pages lang-blog

all: assets $(addprefix build-, $(LANGS))

assets:
	@echo "Copying assets..."
	@$(MKDIR) -p $(PUBLIC_DIR)
	@$(CP) -ar assets $(PUBLIC_DIR)

# Language-specific build targets
build-%:
	@echo "Building language '$*'..."
	@$(MKDIR) -p $(PUBLIC_DIR)/$*
	@$(MKDIR) -p $(PUBLIC_DIR)/$*/post
	@$(MAKE) lang-pages LANG=$*
	@$(MAKE) lang-blog LANG=$*
	@echo "Language $* built successfully."

# Build individual pages for a language
lang-pages:
	@echo " - Building pages for $(LANG)..."
	@$(foreach page,$(wildcard $(CONTENT_DIR)/$(LANG)/*.txt),\
		page_name=$(basename $(notdir $(page))); \
		translation_vars="$(call get_page_translations,$(basename $(notdir $(page))),$(LANG))"; \
		if [ -n "$$translation_vars" ]; then \
			echo "   Building page: $$page_name"; \
			echo "   Translation vars: $$translation_vars"; \
			if [ "$$page_name" = "index" ]; then \
				$(BLOGC_BASE) \
					-D MENU=index \
					-D LANG=$(LANG) \
					$$translation_vars \
					-o $(PUBLIC_DIR)/$(LANG)/index.html \
					-t $(INDEX_TPL) \
					$(page); \
			else \
				$(MKDIR) -p $(PUBLIC_DIR)/$(LANG)/$$page_name; \
				$(BLOGC_BASE) \
					-D MENU=$$page_name \
					-D LANG=$(LANG) \
					-D SETLANG=/$(LANG)/$$page_name \
					$$translation_vars \
					-o $(PUBLIC_DIR)/$(LANG)/$$page_name/index.html \
					-t $(MAIN_TPL) \
					$(page); \
			fi; \
		else \
			echo "   Skipping page: $$page_name (no translation mapping found)"; \
		fi; \
	)

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
			-o $(PUBLIC_DIR)/$(LANG)/post/$$post_id.html \
			-t $(POST_TPL) \
			$(post); \
	)

clean:
	@echo "Cleaning up..."
	@echo "Deleting $(PUBLIC_DIR) directory"
	@rm -rf $(PUBLIC_DIR)

# Serve the site locally
ifneq ($(BLOGC_RUNSERVER),)
serve: all
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
	@echo "Available languages: $(ALL_LANGS)"
	@echo "Active languages: $(LANGS)"
	@echo ""
	@echo "Page mappings:"
	@echo "EDUCATION: $(EDUCATION_PAGES)"
	@echo "ABOUT: $(ABOUT_PAGES)"
	@echo "CONTACT: $(CONTACT_PAGES)"
	@echo ""
	@echo "Example for 'educacio' in Catalan:"
	@echo "$(call get_page_translations,educacio,ca)"
	@echo ""
	@echo "Example for 'education' in English:"
	@echo "$(call get_page_translations,education,en)"

# Default target
.DEFAULT_GOAL := all