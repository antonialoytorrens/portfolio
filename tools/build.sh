#!/bin/bash
set -euo pipefail

AUTHOR=${AUTHOR:-Antoni Aloy Torrens}
BASE_DOMAIN=${BASE_DOMAIN:-https://antonialoytorrens.com}
LANGS=${LANGS:-ca es en}
SRCLANG=${SRCLANG:-ca}
YEAR=${YEAR:-$(date +%Y)}

CONTENT_DIR=${CONTENT_DIR:-content}
I18N_DIR=${I18N_DIR:-i18n}
TEMPLATE_DIR=${TEMPLATE_DIR:-templates}
PUBLIC_DIR=${PUBLIC_DIR:-public}
WORK_DIR=${WORK_DIR:-.work}

SBLG=${SBLG:-sblg}
LOWDOWN=${LOWDOWN:-lowdown --html-no-skiphtml --html-no-escapehtml}
JQ=${JQ:-jq}
MKART=${MKART:-sh tools/mkarticle.sh}

HEAD_TPL=$TEMPLATE_DIR/partials/head.html
FOOT_TPL=$TEMPLATE_DIR/partials/foot.html
ITEM_TPL=$TEMPLATE_DIR/partials/postitem.html

SERVE_HOST=${SERVE_HOST:-127.0.0.1}
SERVE_PORT=${SERVE_PORT:-8080}

REMOTE_USER=${REMOTE_USER:-user}
REMOTE_HOST=${REMOTE_HOST:-example.com}
REMOTE_PATH=${REMOTE_PATH:-/var/www/html}
RSYNC_OPTS=${RSYNC_OPTS:--avz --delete}

# L, J and SB are set by build_one. Bash locals are visible to callees.
val() { $JQ -r --arg k "$1" '.[$k]' "$J"; }
slug() { $JQ -r --arg k "slug.$2" '.[$k]' "$I18N_DIR/$1.json"; }
pagesrc() {
	if [ -f "$CONTENT_DIR/$L/$1.md" ]; then echo "$CONTENT_DIR/$L/$1.md"
	else echo "$CONTENT_DIR/$SRCLANG/$1.md"; fi
}
# i18n json + k=v -> sed script. @@key@@ subst. delim SOH. escape \ & and dots in keys.
flatsed() {
	local D TAB k v kv
	D=$(printf '\001'); TAB=$(printf '\t')
	{ $JQ -r 'to_entries[]|"\(.key)\t\(.value)"' "$J"
	  for kv in "$@"; do printf '%s\t%s\n' "${kv%%=*}" "${kv#*=}"; done
	} | while IFS=$TAB read -r k v; do
		k=${k//./\\.}
		v=${v//\\/\\\\}; v=${v//&/\\&}
		printf 's%s@@%s@@%s%s%sg\n' "$D" "$k" "$D" "$v" "$D"
	done
}
subst() {
	local t=$1; shift
	[ "$t" = - ] && t=/dev/stdin
	sed -f <(flatsed lang="$L" author="$AUTHOR" year="$YEAR" "$@") "$t"
}
assemble() { cat "$HEAD_TPL" "$TEMPLATE_DIR/$1.html" "$FOOT_TPL"; }
linkify_tags() {
	sed -i 's|<span class="sblg-tag">\([^<]*\)</span>|<a href="/'"$L"'/'"$SB"'/tag/\1.html" class="sblg-tag">\1</a>|g' "$1"
}
inject() {
	sed -f <(for arg in "$@"; do
		printf '/@@%s@@/{\nr %s\nd\n}\n' "${arg%%=*}" "${arg#*=}"
	done)
}
# $1 = slug key, empty = home. Words: url.self.<lang>=/<lang>/<slug>/
selfurls() {
	local x s key=${1:-}
	for x in $LANGS; do
		s=""
		[ -n "$key" ] && s="/$(slug "$x" "$key")"
		printf 'url.self.%s=/%s%s/ ' "$x" "$x" "$s"
	done
}
# publish <tpl> <work> <out> <link 0|1> [sblg flags and inputs] -- [inject k=v] -- <subst k=v>
# inject before subst: spliced partials still contain @@key@@.
publish() {
	local tpl="$1" work="$2" out="$3" link="$4"
	shift 4
	local flags=() files=() iargs=()
	while [ $# -gt 0 ] && [ "$1" != "--" ]; do
		case $1 in
			-*) flags+=("$1") ;;
			*)  files+=("$1") ;;
		esac
		shift
	done
	[ $# -gt 0 ] && shift
	while [ $# -gt 0 ] && [ "$1" != "--" ]; do
		iargs+=("$1")
		shift
	done
	[ $# -gt 0 ] && shift
	if [ ${#iargs[@]} -gt 0 ]; then
		assemble "$tpl" | inject "${iargs[@]}" | subst - "$@" > "$work"
	else
		assemble "$tpl" | subst - "$@" > "$work"
	fi
	$SBLG "${flags[@]}" -o "$out" -t "$work" "${files[@]}"
	if [ "$link" = 1 ]; then
		linkify_tags "$out"
	fi
}

assets() {
	echo "==> Copying assets"
	mkdir -p "$PUBLIC_DIR"
	cp -a assets "$PUBLIC_DIR/"
	cat assets/css/reset.css assets/css/styles.css > "$PUBLIC_DIR/assets/css/bundle.css"
	rm -f "$PUBLIC_DIR/assets/css/reset.css" "$PUBLIC_DIR/assets/css/styles.css"
}

build_one() {
	local L=${LANG:-}
	case " $LANGS " in
		*" $L "*) ;;
		*) echo "usage: make build-one LANG=ca|es|en" >&2; exit 1 ;;
	esac
	local J=$I18N_DIR/$L.json
	echo "==> Building language '$L'"
	# Localized post dates: LC_ALL pins the locale, TZ=UTC the calendar day.
	export LC_ALL=$(val date.locale)
	export TZ=UTC

	local SB SE SP SX
	SB=$(val slug.blog); SE=$(val slug.education)
	SP=$(val slug.projects); SX=$(val slug.experience)

	mkdir -p "$WORK_DIR/$L/blog" "$PUBLIC_DIR/$L"
	shopt -s nullglob
	local BLOGSRC=$CONTENT_DIR/$L/blog
	local posts=("$BLOGSRC"/*.md)
	if [ ${#posts[@]} -eq 0 ]; then
		BLOGSRC=$CONTENT_DIR/$SRCLANG/blog
		posts=("$BLOGSRC"/*.md)
	fi
	shopt -u nullglob
	local FRAGS=() f n
	for f in "${posts[@]}"; do
		n=$(basename "$f" .md)
		$MKART "$f" --source "$SB/post/$n.html" -o "$WORK_DIR/$L/blog/$n.xml"
		FRAGS+=("$WORK_DIR/$L/blog/$n.xml")
	done

	$LOWDOWN "$(pagesrc index)" > "$WORK_DIR/$L/intro.html"
	local TH TB THEAD
	TH=$(val title.home)
	publish index "$WORK_DIR/$L/tmpl-index.html" "$PUBLIC_DIR/$L/index.html" 1 \
		"${FRAGS[@]}" -- \
		home_intro=$WORK_DIR/$L/intro.html post_item=$ITEM_TPL -- \
		section=about page_heading="$TH" page_title="$TH" page_description="" $(selfurls)

	build_page() {
		local name=$1 slug=$2
		mkdir -p "$PUBLIC_DIR/$L/$slug"
		$MKART "$(pagesrc "$name")" -o "$WORK_DIR/$L/page-$name.xml"
		publish page "$WORK_DIR/$L/tmpl-$name.html" "$PUBLIC_DIR/$L/$slug/index.html" 0 \
			-c "$WORK_DIR/$L/page-$name.xml" -- -- \
			section=$name page_heading='${sblg-title}' page_title='${sblg-titletext}' \
			page_description='${sblg-aside}' $(selfurls "$name")
	}
	build_page education "$SE"
	build_page projects "$SP"
	build_page experience "$SX"

	mkdir -p "$PUBLIC_DIR/$L/$SB"
	TB=$(val title.blog)
	publish blog "$WORK_DIR/$L/tmpl-blog.html" "$PUBLIC_DIR/$L/$SB/index.html" 1 \
		"${FRAGS[@]}" -- \
		post_item=$ITEM_TPL -- \
		section=blog page_heading="$TB" page_title="$TB" page_description="" $(selfurls blog)

	mkdir -p "$PUBLIC_DIR/$L/$SB/post"
	local fr
	for fr in "${FRAGS[@]}"; do
		n=$(basename "$fr" .xml)
		publish post "$WORK_DIR/$L/tmpl-post.html" "$PUBLIC_DIR/$L/$SB/post/$n.html" 1 \
			-c "$fr" -- -- \
			section=blog page_heading='${sblg-title}' page_title='${sblg-titletext}' \
			page_description='${sblg-aside}' $(selfurls blog)
	done

	mkdir -p "$PUBLIC_DIR/$L/$SB/tag"
	THEAD=$(val tag.heading)
	local t
	for t in $($SBLG -l "${FRAGS[@]}" | cut -f2 | sort -u); do
		[ -n "$t" ] || continue
		publish tag "$WORK_DIR/$L/tmpl-tag-$t.html" "$PUBLIC_DIR/$L/$SB/tag/$t.html" 1 \
			"${FRAGS[@]}" -- \
			post_item=$ITEM_TPL -- \
			section=blog tag="$t" page_heading="$TB" page_title="$THEAD $t" page_description="" $(selfurls blog)
	done

	subst "$TEMPLATE_DIR/atom.in.xml" base_url="$BASE_DOMAIN" > "$WORK_DIR/$L/atom-tmpl.xml"
	$SBLG -a -o "$PUBLIC_DIR/$L/atom.xml" -t "$WORK_DIR/$L/atom-tmpl.xml" "${FRAGS[@]}"

	echo "    language '$L' done."
}

all() {
	assets
	local l
	for l in $LANGS; do
		( LANG=$l build_one )
	done
	echo "Build complete -> $PUBLIC_DIR/"
}

clean() {
	echo "==> Cleaning $PUBLIC_DIR/ and $WORK_DIR/"
	rm -rf "$PUBLIC_DIR" "$WORK_DIR"
}

serve() {
	all
	echo "==> Serving $PUBLIC_DIR/ at http://$SERVE_HOST:$SERVE_PORT/"
	# -D foreground. -h bind addr.
	mini_httpd -D -h "$SERVE_HOST" -p "$SERVE_PORT" -d "$PUBLIC_DIR"
}

sync_site() {
	all
	echo "==> Syncing $PUBLIC_DIR/ to $REMOTE_USER@$REMOTE_HOST:$REMOTE_PATH"
	rsync $RSYNC_OPTS "$PUBLIC_DIR/" "$REMOTE_USER@$REMOTE_HOST:$REMOTE_PATH"
}

help() {
	echo "Targets: all (default), assets, build-one LANG=<lang>, clean, serve, rsync"
	echo "Languages: $LANGS  (source/fallback: $SRCLANG)"
}

cmd=${1:-all}
case $cmd in
	all) all ;;
	assets) assets ;;
	build-one) build_one ;;
	clean) clean ;;
	serve) serve ;;
	rsync) sync_site ;;
	help) help ;;
	*) echo "unknown target: $cmd" >&2; exit 1 ;;
esac
