#!/bin/sh
# md + front matter -> sblg <article>. attrs data-sblg-*. body via lowdown.
# usage: mkarticle.sh <in.md> [-o out.xml] [--source path]
set -eu

LOWDOWN="lowdown --html-no-skiphtml --html-no-escapehtml"

src=""
out=""
source=""
while [ $# -gt 0 ]; do
	case "$1" in
		-o)       out="$2";    shift 2 ;;
		--source) source="$2"; shift 2 ;;
		*)        src="$1";    shift ;;
	esac
done

[ -n "$src" ] || { echo "usage: mkarticle.sh <input.md> [-o out.xml] [--source path]" >&2; exit 1; }

# Where does the body start? Only treat a leading `---` as front matter.
if [ "$(head -n1 "$src")" = "---" ]; then
	bstart=$(awk 'NR>1 && /^---[[:space:]]*$/ { print NR + 1; exit }' "$src")
else
	bstart=1
fi

# Build the opening <article ...> tag (with the optional <aside>) from the
# front matter. All attribute values are XML-escaped.
open=$(awk -v source="$source" '
	function esc(s) {
		gsub(/&/, "\\&amp;", s)
		gsub(/</, "\\&lt;",  s)
		gsub(/>/, "\\&gt;",  s)
		gsub(/"/, "\\&quot;", s)
		return s
	}
	NR == 1 && /^---[[:space:]]*$/ { infm = 1; next }
	infm && /^---[[:space:]]*$/    { infm = 0; exit }
	infm {
		i = index($0, ":")
		if (i == 0) next
		key = substr($0, 1, i - 1)
		val = substr($0, i + 1)
		gsub(/^[ \t]+|[ \t]+$/, "", key)
		gsub(/^[ \t]+|[ \t]+$/, "", val)
		if (key == "") next
		meta[key] = val
		order[++n] = key
	}
	END {
		printf "<article data-sblg-article=\"1\""
		if (source != "")        printf " data-sblg-source=\"%s\"",  esc(source)
		if ("title" in meta)     printf " data-sblg-title=\"%s\"",   esc(meta["title"])
		if ("date" in meta)      printf " data-sblg-datetime=\"%s\"", esc(substr(meta["date"], 1, 10))
		if ("tags" in meta)      printf " data-sblg-tags=\"%s\"",    esc(meta["tags"])
		if ("img" in meta)       printf " data-sblg-img=\"%s\"",     esc(meta["img"])
		for (i = 1; i <= n; i++) {
			k = order[i]
			if (k == "title" || k == "date" || k == "tags" || k == "img" || k == "description") continue
			printf " data-sblg-set-%s=\"%s\"", k, esc(meta[k])
		}
		printf ">\n"
		if ("description" in meta) printf "  <aside>%s</aside>\n", esc(meta["description"])
	}
' "$src")

emit() {
	printf '%s' "$open"
	sed -n "${bstart},\$p" "$src" | $LOWDOWN
	printf '</article>\n'
}

if [ -n "$out" ]; then
	emit > "$out"
else
	emit
fi
