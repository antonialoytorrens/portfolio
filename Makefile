ifneq (,$(wildcard ./.env))
include .env
export
endif

CC       ?= cc
CFLAGS   ?= -std=c99 -Wall -Wextra -pedantic
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L
CJSON_CFLAGS := $(shell pkg-config --cflags libcjson)
CJSON_LIBS   := $(shell pkg-config --libs libcjson)
INCLUDES  = -Itools -Iexternal/stb -Iexternal/unity $(CJSON_CFLAGS)

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
	@echo "make update-external  refresh Unity, stb_ds"
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

bin/build: tools/build.c $(UTIL) tools/util.h | bin
	$(CC) $(CPPFLAGS) $(INCLUDES) $(CFLAGS) -o $@ tools/build.c $(UTIL) $(CJSON_LIBS)

bin/mkarticle: tools/mkarticle.c $(UTIL) tools/util.h | bin
	$(CC) $(CPPFLAGS) $(INCLUDES) $(CFLAGS) -o $@ tools/mkarticle.c $(UTIL) $(CJSON_LIBS)

bin/test_%: tests/test_%.c $(UTIL) tools/util.h $(UNITY) | bin
	$(CC) $(CPPFLAGS) $(INCLUDES) $(CFLAGS) -o $@ tests/test_$*.c $(UTIL) $(UNITY) $(CJSON_LIBS)
