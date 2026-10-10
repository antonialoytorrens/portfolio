# Portfolio — Hugo
#
#   make            build into public/
#   make minify     production build (minified)
#   make serve      local preview (http://127.0.0.1:1313/)
#   make draft      preview including drafts
#   make clean      remove public/ and generated resources
#   make rsync      upload public/ to the server
#   make deploy     minify + rsync
#   make help       list targets

SHELL := /bin/bash

HUGO       ?= hugo
PUBLIC_DIR := public

SERVE_HOST ?= 127.0.0.1
SERVE_PORT ?= 1313

REMOTE      ?= portfolio
REMOTE_PATH ?= /var/www/portfolio
RSYNC       ?= rsync
RSYNC_OPTS  ?= -avE --progress --delete

.PHONY: all build minify serve draft clean rsync deploy help sonarqube coverage-sonar

all: build

build:
	$(HUGO)

minify:
	$(HUGO) --minify

serve:
	$(HUGO) server --bind $(SERVE_HOST) --port $(SERVE_PORT)

draft:
	$(HUGO) server --bind $(SERVE_HOST) --port $(SERVE_PORT) -D

clean:
	rm -rf $(PUBLIC_DIR) resources/_gen .hugo_build.lock

rsync:
	$(RSYNC) $(RSYNC_OPTS) $(PUBLIC_DIR) $(REMOTE):$(REMOTE_PATH)

deploy: minify rsync

help:
	@echo "Targets:"
	@echo "  make / make build   Generate the site into $(PUBLIC_DIR)/"
	@echo "  make minify         Production build (minified HTML/CSS/JS)"
	@echo "  make serve          Preview at http://$(SERVE_HOST):$(SERVE_PORT)/"
	@echo "  make draft          Preview including draft pages"
	@echo "  make clean          Remove $(PUBLIC_DIR)/ and generated resources"
	@echo "  make rsync          rsync $(PUBLIC_DIR) -> $(REMOTE):$(REMOTE_PATH)"
	@echo "  make deploy         minify + rsync"
	@echo "  make help           This list"
	@echo "  make sonarqube      SonarQube analysis"

# --- SonarQube ---
ifneq (,$(wildcard ./.env))
include .env
export
endif

coverage-sonar:
	-coverage run -m pytest -q
	-coverage xml -o coverage.xml

sonarqube: coverage-sonar
	pysonar \
		--sonar-host-url=$(SONAR_HOST_URL) \
		--sonar-token=$(SONAR_TOKEN) \
		--sonar-project-key=$(SONAR_PROJECT_KEY)
