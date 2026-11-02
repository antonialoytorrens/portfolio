# targets -> tools/build.sh. CLI vars via env.
ifneq (,$(wildcard ./.env))
include .env
export
endif

.PHONY: all assets build-one clean serve rsync help dockershell

all assets build-one clean serve rsync:
	@bash tools/build.sh $@

help:
	@bash tools/build.sh help
	@echo "make dockershell: shell in portfolio-ci container"

dockershell:
	docker compose run --service-ports --rm portfolio-ci /bin/bash
