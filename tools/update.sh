#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

fetch() {
	out=$1
	url=$2
	mkdir -p "$(dirname "$out")"
	curl -fsSL "$url" -o "$out"
}

fetch external/cJSON/cJSON.c \
	https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c
fetch external/cJSON/cJSON.h \
	https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
fetch external/cJSON/LICENSE \
	https://raw.githubusercontent.com/DaveGamble/cJSON/master/LICENSE

fetch external/unity/unity.c \
	https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity.c
fetch external/unity/unity.h \
	https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity.h
fetch external/unity/unity_internals.h \
	https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity_internals.h
fetch external/unity/LICENSE.txt \
	https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/LICENSE.txt

fetch external/stb/stb_ds.h \
	https://raw.githubusercontent.com/nothings/stb/master/stb_ds.h
