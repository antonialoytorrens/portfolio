#!/usr/bin/make -f

ifneq (,$(wildcard ./.env))
include .env
export
endif

ifeq ($(origin CC),default)
CC = clang
endif
LLVM_COV       ?= llvm-cov
LLVM_PROFDATA  ?= llvm-profdata
CLANG_TIDY     ?= clang-tidy
CPPCHECK       ?= cppcheck
FLAWFINDER     ?= flawfinder
LCOV2COBERTURA ?= lcov2xml
SONAR_SCANNER  ?= sonar-scanner
MINI_HTTPD     ?= mini_httpd
HOST           ?= 127.0.0.1
PORT           ?= 8080
CLANG_RESOURCE_INCLUDE := $(shell $(CC) -print-resource-dir)/include
SONAR_CXX_INCLUDES = tools,external/stb,/usr/include,/usr/include/x86_64-linux-gnu,$(CLANG_RESOURCE_INCLUDE)

CFLAGS   ?= -std=c99 -Wall -Wextra -pedantic
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L
CJSON_CFLAGS := $(shell pkg-config --cflags libcjson 2>/dev/null)
CJSON_LIBS   := $(shell pkg-config --libs libcjson 2>/dev/null)
INCLUDES  = -Itools -Iexternal/stb -Iexternal/unity $(CJSON_CFLAGS)

TEST_CFLAGS  = $(CPPFLAGS) $(INCLUDES) $(CFLAGS) -g -O0 -DCOVERAGE \
	-fprofile-instr-generate -fcoverage-mapping
TEST_LDFLAGS = -fprofile-instr-generate

PUBLIC_DIR ?= public
WORK_DIR   ?= .work
BUILD_DIR   = build
OBJ_DIR     = $(BUILD_DIR)/obj
REPORT_DIR  = $(BUILD_DIR)/report
PROF_DIR    = $(BUILD_DIR)/prof
COMPILE_DB  = $(BUILD_DIR)/compile_commands.json
LH_DIR      = $(REPORT_DIR)/lh

C_SRC = tools/build.c tools/mkarticle.c tools/lighthouse_to_sonar.c \
	tools/util.c tools/kv.c tools/fs.c tools/proc.c
C_OBJ = $(patsubst tools/%.c,$(OBJ_DIR)/%.o,$(C_SRC))

UNITY = external/unity/unity.c
UTIL  = tools/util.c tools/kv.c tools/fs.c tools/proc.c
COV_BINS = bin/build-cov bin/mkarticle-cov
TEST_BINS = bin/test_json bin/test_subst bin/test_mkarticle bin/test_date bin/test_linkify \
	bin/test_fs bin/test_proc bin/test_cli

SRCLANG ?= ca

.PHONY: all assets build-one clean serve run rsync help test update-external \
	docker-build dockershell sonarqube coverage scan-cppcheck scan-clang-tidy \
	scan-flawfinder scan-sonar lighthouse lh-sonar

all: bin/build bin/mkarticle
	./bin/build all

assets: bin/build
	./bin/build assets

build-one: bin/build bin/mkarticle
	./bin/build build-one

clean:
	rm -rf $(PUBLIC_DIR) $(WORK_DIR) $(BUILD_DIR)
	rm -f bin/build bin/mkarticle bin/lighthouse_to_sonar $(TEST_BINS) $(COV_BINS)

serve: bin/build bin/mkarticle
	./bin/build serve

run:
	$(MINI_HTTPD) -D -h $(HOST) -p $(PORT) -d $(PUBLIC_DIR)

rsync: bin/build bin/mkarticle
	./bin/build rsync

help: bin/build
	./bin/build help
	@echo "make test             unit tests"
	@echo "make coverage         Cobertura XML for SonarQube"
	@echo "make scan-sonar       cppcheck, clang-tidy, flawfinder"
	@echo "make run              serve public/ with mini_httpd (HOST/PORT)"
	@echo "make lighthouse       Unlighthouse crawl (ephemeral port)"
	@echo "make sonarqube        cxx reports, coverage, lighthouse, sonar-scanner"
	@echo "make update-external  refresh external dependencies"
	@echo "make docker-build     build portfolio-ci image"
	@echo "make dockershell      shell in portfolio-ci container"

docker-build:
	docker compose build

test: $(TEST_BINS) $(COV_BINS) | $(PROF_DIR)
	rm -f $(PROF_DIR)/*.profraw
	@for bin in $(TEST_BINS); do \
		LLVM_PROFILE_FILE="$(CURDIR)/$(PROF_DIR)/$$(basename $$bin)-%p.profraw" $$bin || exit 1; \
	done

update-external:
	sh tools/update.sh

dockershell:
	docker compose run --service-ports --rm portfolio-ci /bin/bash

coverage: test | $(REPORT_DIR)
	$(LLVM_PROFDATA) merge -sparse \
		$(PROF_DIR)/*.profraw \
		-o $(PROF_DIR)/coverage.profdata
	$(LLVM_COV) export \
		-format=lcov \
		-instr-profile=$(PROF_DIR)/coverage.profdata \
		$(addprefix -object=,$(TEST_BINS) $(COV_BINS)) \
		> $(REPORT_DIR)/coverage.lcov
	$(LCOV2COBERTURA) -b $(CURDIR) \
		-e '^external' \
		-o $(REPORT_DIR)/coverage.xml \
		$(REPORT_DIR)/coverage.lcov

scan-cppcheck: $(COMPILE_DB) | $(REPORT_DIR)
	$(CPPCHECK) --project=$(COMPILE_DB) --enable=all --inline-suppr \
		--xml --xml-version=2 --suppress=missingIncludeSystem \
		--suppress=checkersReport \
		--suppress='*:external/*' \
		--check-level=exhaustive \
		--quiet --checkers-report=$(REPORT_DIR)/cppcheck-checkers-report.txt \
		> $(REPORT_DIR)/cppcheck-report.xml 2>&1

scan-clang-tidy: $(COMPILE_DB) | $(REPORT_DIR)
	$(CLANG_TIDY) -p $(BUILD_DIR) --quiet \
		--header-filter='(.*/)?tools/.*' \
		--extra-arg=-Qunused-arguments \
		$(C_SRC) > $(REPORT_DIR)/clang-tidy-report.txt 2>&1

scan-flawfinder: | $(REPORT_DIR)
	$(FLAWFINDER) --columns --dataonly $(C_SRC) \
		> $(REPORT_DIR)/flawfinder-report.txt

scan-sonar: scan-cppcheck scan-clang-tidy scan-flawfinder

# NOTE: no PWA scan. Ephemeral port = 8080 + (0..999).
RANDOM_SUFFIX := $(shell bash -c 'echo $$RANDOM')
LH_PORT       := $(shell bash -c 'echo $$((8080 + $(RANDOM_SUFFIX) % 1000))')

lighthouse: all
	rm -rf $(LH_DIR) && mkdir -p $(LH_DIR)/desktop $(LH_DIR)/mobile
	@set -e; \
	$(MINI_HTTPD) -D -h $(HOST) -p $(LH_PORT) -d $(PUBLIC_DIR) & \
	LH_PID=$$!; \
	trap 'kill $$LH_PID 2>/dev/null' EXIT; \
	sleep 0.2; \
	CI=true unlighthouse-ci \
		--site "http://$(HOST):$(LH_PORT)/$(SRCLANG)/" \
		--desktop \
		--no-cache \
		--build-static \
		--config-file unlighthouse.config.mjs \
		--output-path "$(LH_DIR)/desktop" \
		--reporter jsonExpanded; \
	CI=true unlighthouse-ci \
		--site "http://$(HOST):$(LH_PORT)/$(SRCLANG)/" \
		--mobile \
		--no-cache \
		--build-static \
		--config-file unlighthouse.config.mjs \
		--output-path "$(LH_DIR)/mobile" \
		--reporter jsonExpanded

# I have considered writing a SonarQube plugin for Lighthouse
# but I think the software is not prepared for non-source code files.
# See: https://community.sonarsource.com/t/how-to-add-measure-on-a-virtual-resource/58304
lh-sonar: lighthouse bin/lighthouse_to_sonar
	./bin/lighthouse_to_sonar

sonarqube: scan-sonar coverage lh-sonar
	$(SONAR_SCANNER) \
		-Dsonar.host.url=$(SONAR_HOST_URL) \
		-Dsonar.token=$(SONAR_TOKEN) \
		-Dsonar.projectKey=$(SONAR_PROJECT_KEY) \
		-Dsonar.cxx.includeDirectories=$(SONAR_CXX_INCLUDES)

bin $(WORK_DIR) $(OBJ_DIR) $(REPORT_DIR) $(PROF_DIR):
	mkdir -p $@

$(COMPILE_DB): $(C_OBJ)
	@sed -e '1s/^/[/' -e '$$s/,$$/]/' $(C_OBJ:%.o=%.o.json) > $@

$(OBJ_DIR)/%.o: tools/%.c tools/util.h | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(INCLUDES) $(CFLAGS) -MJ $@.json -c -o $@ $<

LIB_OBJ = $(OBJ_DIR)/util.o $(OBJ_DIR)/kv.o $(OBJ_DIR)/fs.o $(OBJ_DIR)/proc.o

bin/build: $(OBJ_DIR)/build.o $(LIB_OBJ) | bin
	$(CC) -o $@ $^ $(CJSON_LIBS)

bin/mkarticle: $(OBJ_DIR)/mkarticle.o $(LIB_OBJ) | bin
	$(CC) -o $@ $^ $(CJSON_LIBS)

bin/lighthouse_to_sonar: $(OBJ_DIR)/lighthouse_to_sonar.o $(LIB_OBJ) | bin
	$(CC) -o $@ $^ $(CJSON_LIBS)

bin/test_%: tests/test_%.c $(UTIL) tools/util.h $(UNITY) | bin
	$(CC) $(TEST_CFLAGS) $(TEST_LDFLAGS) -o $@ tests/test_$*.c $(UTIL) $(UNITY) $(CJSON_LIBS)

bin/build-cov: tools/build.c $(UTIL) tools/util.h | bin
	$(CC) $(TEST_CFLAGS) $(TEST_LDFLAGS) -o $@ tools/build.c $(UTIL) $(CJSON_LIBS)

bin/mkarticle-cov: tools/mkarticle.c $(UTIL) tools/util.h | bin
	$(CC) $(TEST_CFLAGS) $(TEST_LDFLAGS) -o $@ tools/mkarticle.c $(UTIL) $(CJSON_LIBS)
