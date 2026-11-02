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
WORK_DIR=${WORK_DIR:-$PUBLIC_DIR/.work}

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

assets() {
	echo "==> Copying assets"
	mkdir -p "$PUBLIC_DIR"
	cp -a assets "$PUBLIC_DIR/"
	cat assets/css/reset.css assets/css/styles.css > "$PUBLIC_DIR/assets/css/bundle.css"
}

build_one() {
	local L=${LANG:-}
	case " $LANGS " in
		*" $L "*) ;;
		*) echo "usage: make build-one LANG=ca|es|en" >&2; exit 1 ;;
	esac
	local J=$I18N_DIR/$L.json
	echo "==> Building language '$L'"

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

	local SB SE SP SX x
	SB=$(val slug.blog); SE=$(val slug.education)
	SP=$(val slug.projects); SX=$(val slug.experience)
	declare -A BLOG EDU PROJ EXP
	for x in $LANGS; do
		BLOG[$x]=$(slug "$x" blog);       EDU[$x]=$(slug "$x" education)
		PROJ[$x]=$(slug "$x" projects);   EXP[$x]=$(slug "$x" experience)
	done
	local sw_home sw_blog sw_edu sw_proj sw_exp
	sw_home="url.self.ca=/ca/ url.self.es=/es/ url.self.en=/en/"
	sw_blog="url.self.ca=/ca/${BLOG[ca]}/ url.self.es=/es/${BLOG[es]}/ url.self.en=/en/${BLOG[en]}/"
	sw_edu="url.self.ca=/ca/${EDU[ca]}/ url.self.es=/es/${EDU[es]}/ url.self.en=/en/${EDU[en]}/"
	sw_proj="url.self.ca=/ca/${PROJ[ca]}/ url.self.es=/es/${PROJ[es]}/ url.self.en=/en/${PROJ[en]}/"
	sw_exp="url.self.ca=/ca/${EXP[ca]}/ url.self.es=/es/${EXP[es]}/ url.self.en=/en/${EXP[en]}/"

	mkdir -p "$WORK_DIR/$L/blog" "$PUBLIC_DIR/$L"
	local BLOGSRC=$CONTENT_DIR/$L/blog
	ls $BLOGSRC/*.md >/dev/null 2>&1 || BLOGSRC=$CONTENT_DIR/$SRCLANG/blog
	local FRAGS=() f n
	if ls $BLOGSRC/*.md >/dev/null 2>&1; then
		for f in $BLOGSRC/*.md; do
			n=$(basename "$f" .md)
			$MKART "$f" --source "$SB/post/$n.html" -o "$WORK_DIR/$L/blog/$n.xml"
			FRAGS+=("$WORK_DIR/$L/blog/$n.xml")
		done
	fi

	$LOWDOWN "$(pagesrc index)" > "$WORK_DIR/$L/intro.html"
	local TH TB THEAD
	TH=$(val title.home)
	assemble index \
		| inject home_intro=$WORK_DIR/$L/intro.html post_item=$ITEM_TPL \
		| subst - section=about page_heading="$TH" page_title="$TH" page_description="" $sw_home \
		> "$WORK_DIR/$L/tmpl-index.html"
	$SBLG -o "$PUBLIC_DIR/$L/index.html" -t "$WORK_DIR/$L/tmpl-index.html" "${FRAGS[@]}"
	linkify_tags "$PUBLIC_DIR/$L/index.html"

	build_page() {
		local sec=$1 name=$2 slug=$3 sw=$4
		mkdir -p "$PUBLIC_DIR/$L/$slug"
		$MKART "$(pagesrc "$name")" -o "$WORK_DIR/$L/page-$name.xml"
		assemble page \
			| subst - \
				section=$sec page_heading='${sblg-title}' page_title='${sblg-titletext}' \
				page_description='${sblg-aside}' $sw \
			> "$WORK_DIR/$L/tmpl-$name.html"
		$SBLG -c -o "$PUBLIC_DIR/$L/$slug/index.html" \
			-t "$WORK_DIR/$L/tmpl-$name.html" "$WORK_DIR/$L/page-$name.xml"
	}
	build_page education education "$SE" "$sw_edu"
	build_page projects   projects   "$SP" "$sw_proj"
	build_page experience experience "$SX" "$sw_exp"

	mkdir -p "$PUBLIC_DIR/$L/$SB"
	TB=$(val title.blog)
	assemble blog \
		| inject post_item=$ITEM_TPL \
		| subst - section=blog page_heading="$TB" page_title="$TB" page_description="" $sw_blog \
		> "$WORK_DIR/$L/tmpl-blog.html"
	$SBLG -o "$PUBLIC_DIR/$L/$SB/index.html" -t "$WORK_DIR/$L/tmpl-blog.html" "${FRAGS[@]}"
	linkify_tags "$PUBLIC_DIR/$L/$SB/index.html"

	mkdir -p "$PUBLIC_DIR/$L/$SB/post"
	assemble post \
		| subst - \
			section=blog page_heading='${sblg-title}' page_title='${sblg-titletext}' \
			page_description='${sblg-aside}' $sw_blog \
		> "$WORK_DIR/$L/tmpl-post.html"
	local fr
	for fr in "${FRAGS[@]}"; do
		n=$(basename "$fr" .xml)
		$SBLG -c -o "$PUBLIC_DIR/$L/$SB/post/$n.html" -t "$WORK_DIR/$L/tmpl-post.html" "$fr"
		linkify_tags "$PUBLIC_DIR/$L/$SB/post/$n.html"
	done

	mkdir -p "$PUBLIC_DIR/$L/$SB/tag"
	THEAD=$(val tag.heading)
	local t
	for t in $($SBLG -l "${FRAGS[@]}" | cut -f2 | sort -u); do
		[ -n "$t" ] || continue
		assemble tag \
			| inject post_item=$ITEM_TPL \
			| subst - \
				section=blog tag="$t" page_heading="$TB" page_title="$THEAD $t" page_description="" $sw_blog \
			> "$WORK_DIR/$L/tmpl-tag-$t.html"
		$SBLG -o "$PUBLIC_DIR/$L/$SB/tag/$t.html" -t "$WORK_DIR/$L/tmpl-tag-$t.html" "${FRAGS[@]}"
		linkify_tags "$PUBLIC_DIR/$L/$SB/tag/$t.html"
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
	echo "==> Cleaning $PUBLIC_DIR/"
	rm -rf "$PUBLIC_DIR"
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
