ifneq (,$(wildcard ./.env))
include .env
export
endif

CC       ?= cc
CFLAGS   ?= -std=c99 -Wall -Wextra -pedantic
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L
INCLUDES  = -Itools -Iexternal/cJSON -Iexternal/stb -Iexternal/unity

JSON  = external/cJSON/cJSON.c
UNITY = external/unity/unity.c
UTIL  = tools/util.c

.PHONY: all assets build-one clean serve rsync help test update-external dockershell

all: bin/build bin/mkarticle
	./bin/build all

assets: bin/build
	./bin/build assets

build-one: bin/build bin/mkarticle
	./bin/build build-one

clean: bin/build
	./bin/build clean

serve: bin/build bin/mkarticle
	./bin/build serve

rsync: bin/build bin/mkarticle
	./bin/build rsync

help: bin/build
	./bin/build help
	@echo "make test             unit tests"
	@echo "make update-external  refresh cJSON, Unity, stb"
	@echo "make dockershell      shell in portfolio-ci container"

test: bin/test_json bin/test_subst bin/test_mkarticle bin/test_date bin/test_linkify
	./bin/test_json
	./bin/test_subst
	./bin/test_mkarticle
	./bin/test_date
	./bin/test_linkify

update-external:
	sh tools/update.sh

dockershell:
	docker compose run --service-ports --rm portfolio-ci /bin/bash

bin:
	mkdir -p bin

bin/build: tools/build.c $(UTIL) tools/util.h $(JSON) | bin
	$(CC) $(CPPFLAGS) $(INCLUDES) $(CFLAGS) -o $@ tools/build.c $(UTIL) $(JSON)

bin/mkarticle: tools/mkarticle.c $(UTIL) tools/util.h $(JSON) | bin
	$(CC) $(CPPFLAGS) $(INCLUDES) $(CFLAGS) -o $@ tools/mkarticle.c $(UTIL) $(JSON)

bin/test_%: tests/test_%.c $(UTIL) tools/util.h $(JSON) $(UNITY) | bin
	$(CC) $(CPPFLAGS) $(INCLUDES) $(CFLAGS) -o $@ tests/test_$*.c $(UTIL) $(JSON) $(UNITY)
