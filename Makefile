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

CFLAGS   ?= -std=c99 -Wall -Wextra -pedantic
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L
CJSON_CFLAGS := $(shell pkg-config --cflags libcjson 2>/dev/null)
CJSON_LIBS   := $(shell pkg-config --libs libcjson 2>/dev/null)
INCLUDES  = -Itools -Iexternal/stb -Iexternal/unity $(CJSON_CFLAGS)

TEST_CFLAGS  = $(CPPFLAGS) $(INCLUDES) $(CFLAGS) -g -O0 \
	-fprofile-instr-generate -fcoverage-mapping
TEST_LDFLAGS = -fprofile-instr-generate

PUBLIC_DIR ?= public
WORK_DIR   ?= .work
BUILD_DIR  = build
OBJ_DIR    = $(BUILD_DIR)/obj
REPORT_DIR = $(BUILD_DIR)/report
PROF_DIR   = $(BUILD_DIR)/prof
COMPILE_DB = $(BUILD_DIR)/compile_commands.json

C_SRC = tools/build.c tools/mkarticle.c tools/util.c
C_OBJ = $(patsubst tools/%.c,$(OBJ_DIR)/%.o,$(C_SRC))

UNITY = external/unity/unity.c
UTIL  = tools/util.c
TEST_BINS = bin/test_json bin/test_subst bin/test_mkarticle bin/test_date bin/test_linkify

.PHONY: all assets build-one clean serve rsync help test update-external \
	docker-build dockershell sonarqube coverage scan-cppcheck scan-clang-tidy \
	scan-flawfinder scan-sonar

all: bin/build bin/mkarticle
	./bin/build all

assets: bin/build
	./bin/build assets

build-one: bin/build bin/mkarticle
	./bin/build build-one

clean:
	rm -rf $(PUBLIC_DIR) $(WORK_DIR) $(BUILD_DIR)
	rm -f bin/build bin/mkarticle $(TEST_BINS)

serve: bin/build bin/mkarticle
	./bin/build serve

rsync: bin/build bin/mkarticle
	./bin/build rsync

help: bin/build
	./bin/build help
	@echo "make test             unit tests"
	@echo "make coverage         Cobertura XML for SonarQube"
	@echo "make scan-sonar       cppcheck, clang-tidy, flawfinder"
	@echo "make sonarqube        reports, coverage, sonar-scanner"
	@echo "make update-external  refresh Unity, stb_ds"
	@echo "make docker-build     build portfolio-ci image"
	@echo "make dockershell      shell in portfolio-ci container"

docker-build:
	docker compose build

test: $(TEST_BINS) | $(PROF_DIR)
	@for bin in $(TEST_BINS); do \
		LLVM_PROFILE_FILE="$(PROF_DIR)/$$(basename $$bin).profraw" $$bin || exit 1; \
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
		$(addprefix -object=,$(TEST_BINS)) \
		> $(REPORT_DIR)/coverage.lcov
	$(LCOV2COBERTURA) -b $(CURDIR) \
		-e '^external' \
		-o $(REPORT_DIR)/coverage.xml \
		$(REPORT_DIR)/coverage.lcov

scan-cppcheck: $(COMPILE_DB) | $(REPORT_DIR)
	$(CPPCHECK) --project=$(COMPILE_DB) --enable=all \
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

sonarqube: scan-sonar coverage
	$(SONAR_SCANNER) \
		-Dsonar.host.url=$(SONAR_HOST_URL) \
		-Dsonar.token=$(SONAR_TOKEN) \
		-Dsonar.projectKey=$(SONAR_PROJECT_KEY)

bin $(OBJ_DIR) $(REPORT_DIR) $(PROF_DIR):
	mkdir -p $@

$(COMPILE_DB): $(C_OBJ)
	@sed -e '1s/^/[/' -e '$$s/,$$/]/' $(C_OBJ:%.o=%.o.json) > $@

$(OBJ_DIR)/%.o: tools/%.c tools/util.h | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(INCLUDES) $(CFLAGS) -MJ $@.json -c -o $@ $<

bin/build: $(OBJ_DIR)/build.o $(OBJ_DIR)/util.o | bin
	$(CC) -o $@ $^ $(CJSON_LIBS)

bin/mkarticle: $(OBJ_DIR)/mkarticle.o $(OBJ_DIR)/util.o | bin
	$(CC) -o $@ $^ $(CJSON_LIBS)

bin/test_%: tests/test_%.c $(UTIL) tools/util.h $(UNITY) | bin
	$(CC) $(TEST_CFLAGS) $(TEST_LDFLAGS) -o $@ tests/test_$*.c $(UTIL) $(UNITY) $(CJSON_LIBS)
