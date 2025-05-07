#!/usr/bin/env make -f
# System configuration
SHELL := /bin/sh

# Project configuration
AUTHOR_NAME := Antoni Aloy Torrens
SITE_TITLE = $(AUTHOR_NAME)
BASE_DOMAIN := https://antonialoytorrens.com
CONTENT_DIR := content
LANGUAGES := ca

# NOTE: I will use blogcfiles because it is easier to use.
# In case that we want to design more than one blog, a more traditional
# Makefile and granular approach is needed.
# See: https://github.com/blogc/blogc-example
BLOGCFILES := $(foreach lang,$(LANGUAGES),$(CONTENT_DIR)/$(lang)/blogcfile)

$(CONTENT_DIR)/%/blogcfile: $(CONTENT_DIR)/%/blogcfile.in
	@echo "Generating $@"
	@sed -e 's=@AUTHOR_NAME@=$(AUTHOR_NAME)=g' \
	    -e 's=@SITE_TITLE@=$(SITE_TITLE)=g' \
	    -e 's=@BASE_DOMAIN@=$(BASE_DOMAIN)=g' \
	    $< > $@

genblogc: $(BLOGCFILES)

build: genblogc
	@mkdir -p _build
	@for lang in $(LANGUAGES); do \
		echo "Building blog for $$lang"; \
		blogc-make all -f $(CONTENT_DIR)/$$lang/blogcfile; \
		mv $(CONTENT_DIR)/$$lang/_build _build/$$lang; \
	done
	cp -avr assets _build

clean:
	@rm -rf _build
	@for lang in $(LANGUAGES); do \
		rm -vf $(CONTENT_DIR)/$$lang/blogcfile; \
	done

runserver:
	blogc-runserver _build

all: build

.PHONY: all clean