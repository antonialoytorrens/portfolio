#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
	char *s;
	size_t n;
	size_t cap;
} buf;

static void buf_grow(buf *b, size_t need)
{
	size_t c;
	char *s;

	if (b->n + need < b->n)
		die("oom");
	if (b->n + need <= b->cap)
		return;
	c = b->cap ? b->cap : 64;
	while (c < b->n + need) {
		if (c > (size_t)-1 / 2)
			die("oom");
		c *= 2;
	}
	s = realloc(b->s, c);
	if (!s)
		die("oom");
	b->s = s;
	b->cap = c;
}

static void buf_add(buf *b, const void *p, size_t n)
{
	buf_grow(b, n + 1);
	if (n) {
		if (!b->s || !p)
			die("oom");
		memcpy(b->s + b->n, p, n);
	}
	b->n += n;
	if (!b->s)
		die("oom");
	b->s[b->n] = 0;
}

static void buf_str(buf *b, const char *s)
{
	buf_add(b, s, strlen(s));
}

static char *buf_take(buf *b)
{
	if (!b->s)
		buf_add(b, "", 0);
	return b->s;
}

static char *xml_escape(const char *s)
{
	buf b = {0};
	const unsigned char *p;

	for (p = (const unsigned char *)s; *p; p++) {
		if (*p == '&')
			buf_str(&b, "&amp;");
		else if (*p == '<')
			buf_str(&b, "&lt;");
		else if (*p == '>')
			buf_str(&b, "&gt;");
		else if (*p == '"')
			buf_str(&b, "&quot;");
		else
			buf_add(&b, p, 1);
	}
	return buf_take(&b);
}

static void subst_token(buf *b, kvmap *m, const char *key)
{
	const char *val = kv_get(m, key);

	if (val) {
		buf_str(b, val);
		return;
	}
	buf_str(b, "@@");
	buf_str(b, key);
	buf_str(b, "@@");
}

int subst_apply(const char *in, kvmap *m, char **out)
{
	buf b = {0};
	const char *p;

	if (!in)
		return -1;
	for (p = in; *p;) {
		const char *end;
		char *key;

		if (p[0] != '@' || p[1] != '@') {
			buf_add(&b, p, 1);
			p++;
			continue;
		}
		end = strstr(p + 2, "@@");
		if (!end) {
			buf_str(&b, p);
			break;
		}
		key = malloc((size_t)(end - (p + 2)) + 1);
		if (!key)
			die("oom");
		memcpy(key, p + 2, (size_t)(end - (p + 2)));
		key[end - (p + 2)] = 0;
		subst_token(&b, m, key);
		free(key);
		p = end + 2;
	}
	*out = buf_take(&b);
	return 0;
}

static const char *find_placeholder(const char *line, size_t linelen,
	const char *pat, size_t plen)
{
	size_t k;

	if (linelen < plen)
		return NULL;
	for (k = 0; k + plen <= linelen; k++) {
		if (memcmp(line + k, pat, plen) == 0)
			return line + k;
	}
	return NULL;
}

static int inject_one(buf *b, const char *line, size_t linelen, const inject_item *item)
{
	char *pat;
	size_t plen;
	const char *hitp;
	char *file;
	size_t flen = 0;

	plen = strlen(item->key) + 4;
	pat = malloc(plen + 1);
	if (!pat)
		die("oom");
	snprintf(pat, plen + 1, "@@%s@@", item->key);
	hitp = find_placeholder(line, linelen, pat, plen);
	free(pat);
	if (!hitp)
		return 0;
	file = read_file(item->path, &flen);
	if (!file)
		return -1;
	buf_add(b, file, flen);
	free(file);
	return 1;
}

static int inject_hit(buf *b, const char *line, size_t linelen,
	const inject_item *items, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		int rc = inject_one(b, line, linelen, &items[i]);
		if (rc != 0)
			return rc;
	}
	return 0;
}

static void copy_line(buf *b, const char *line, size_t linelen, int nl)
{
	buf_add(b, line, linelen);
	if (nl)
		buf_add(b, "\n", 1);
}

int inject_apply(const char *in, const inject_item *items, size_t n, char **out)
{
	buf b = {0};
	const char *p;

	if (!in)
		return -1;
	for (p = in; *p;) {
		const char *nl = strchr(p, '\n');
		size_t linelen = nl ? (size_t)(nl - p) : strlen(p);
		int hit = inject_hit(&b, p, linelen, items, n);

		if (hit < 0) {
			free(b.s);
			return -1;
		}
		if (!hit)
			copy_line(&b, p, linelen, nl != NULL);
		p = nl ? nl + 1 : p + linelen;
	}
	*out = buf_take(&b);
	return 0;
}

static int leap(int y)
{
	return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int dim(int y, int m)
{
	static const int d[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	if (m == 2 && leap(y))
		return 29;
	if (m < 1 || m > 12)
		return 0;
	return d[m];
}

static int ymd_chars_ok(const char *s)
{
	int i;

	for (i = 0; i < 10; i++) {
		if (i == 4 || i == 7) {
			if (s[i] != '-')
				return -1;
		} else if (s[i] < '0' || s[i] > '9')
			return -1;
	}
	return 0;
}

static int parse_ymd(const char *s, int *y, int *m, int *d)
{
	if (!s || strlen(s) < 10)
		return -1;
	if (ymd_chars_ok(s) != 0)
		return -1;
	*y = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
	*m = (s[5] - '0') * 10 + (s[6] - '0');
	*d = (s[8] - '0') * 10 + (s[9] - '0');
	if (*d < 1 || *d > dim(*y, *m))
		return -1;
	return 0;
}

static int lang_kind(const char *lang)
{
	if (!lang)
		return -1;
	if (strcmp(lang, "ca") == 0)
		return 0;
	if (strcmp(lang, "es") == 0)
		return 1;
	if (strcmp(lang, "en") == 0)
		return 2;
	return -1;
}

static const char *const *month_names(int kind)
{
	static const char *const ca[] = {
		"gener", "febrer", "març", "abril", "maig", "juny",
		"juliol", "agost", "setembre", "octubre", "novembre", "desembre"
	};
	static const char *const es[] = {
		"enero", "febrero", "marzo", "abril", "mayo", "junio",
		"julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre"
	};
	static const char *const en[] = {
		"January", "February", "March", "April", "May", "June",
		"July", "August", "September", "October", "November", "December"
	};

	if (kind == 0)
		return ca;
	if (kind == 1)
		return es;
	return en;
}

static int format_kind(int kind, const char *const *names, int y, int m, int d,
	char *out, size_t outsz)
{
	int n;

	if (kind == 2)
		n = snprintf(out, outsz, "%s %d, %d", names[m - 1], d, y);
	else if (kind == 1)
		n = snprintf(out, outsz, "%d de %s de %d", d, names[m - 1], y);
	else
		n = snprintf(out, outsz, "%d %s de %d", d, names[m - 1], y);
	if (n < 0 || (size_t)n >= outsz)
		return -1;
	return 0;
}

int format_date_ymd(const char *lang, const char *ymd, char *out, size_t outsz)
{
	int y, m, d, kind;

	if (parse_ymd(ymd, &y, &m, &d) != 0)
		return -1;
	kind = lang_kind(lang);
	if (kind < 0)
		return -1;
	return format_kind(kind, month_names(kind), y, m, d, out, outsz);
}

int year_from_time(time_t t)
{
	struct tm tm;

	if (!gmtime_r(&t, &tm))
		return -1;
	return tm.tm_year + 1900;
}

static int known_lang(const char *lang)
{
	return lang && (strcmp(lang, "ca") == 0 || strcmp(lang, "es") == 0 ||
		strcmp(lang, "en") == 0);
}

static int is_fence(const char *s, size_t n)
{
	size_t i;

	if (n && s[n - 1] == '\r')
		n--;
	if (n < 3 || memcmp(s, "---", 3) != 0)
		return 0;
	for (i = 3; i < n; i++) {
		if (s[i] != ' ' && s[i] != '\t')
			return 0;
	}
	return 1;
}

static char *trim_dup(const char *s, size_t n)
{
	while (n && (s[0] == ' ' || s[0] == '\t')) {
		s++;
		n--;
	}
	while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r'))
		n--;
	char *o = malloc(n + 1);

	if (!o)
		die("oom");
	memcpy(o, s, n);
	o[n] = 0;
	return o;
}

static int key_is(const char *k, const char *lit)
{
	return strcmp(k, lit) == 0;
}

static void attr(buf *b, const char *name, const char *value)
{
	char *esc = xml_escape(value ? value : "");

	buf_str(b, " ");
	buf_str(b, name);
	buf_str(b, "=\"");
	buf_str(b, esc);
	buf_str(b, "\"");
	free(esc);
}

struct fm {
	const char *title;
	const char *date;
	const char *tags;
	const char *img;
	const char *description;
};

static int lang_rejected(const char *lang)
{
	return lang && lang[0] && !known_lang(lang);
}

static void fm_line(strvec *keys, strvec *vals, const char *line, size_t linelen)
{
	const char *colon;
	char *key;
	char *val;

	colon = memchr(line, ':', linelen);
	if (!colon)
		return;
	key = trim_dup(line, (size_t)(colon - line));
	val = trim_dup(colon + 1, linelen - (size_t)(colon + 1 - line));
	if (key[0]) {
		strvec_push(keys, key);
		strvec_push(vals, val);
	}
	free(key);
	free(val);
}

static int open_fence(const char *text, size_t len, size_t *off)
{
	const char *nl;
	size_t n;

	*off = 0;
	if (len < 3)
		return 0;
	nl = memchr(text, '\n', len);
	n = nl ? (size_t)(nl - text) : len;
	if (!is_fence(text, n))
		return 0;
	*off = nl ? n + 1 : len;
	return 1;
}

static size_t front_matter(const char *text, size_t len, strvec *keys, strvec *vals)
{
	size_t off = 0;
	int infm;

	infm = open_fence(text, len, &off);
	while (infm && off < len) {
		const char *line = text + off;
		const char *nl = memchr(line, '\n', len - off);
		size_t linelen = nl ? (size_t)(nl - line) : len - off;

		if (is_fence(line, linelen))
			return nl ? (size_t)(nl - text) + 1 : len;
		fm_line(keys, vals, line, linelen);
		off = nl ? (size_t)(nl - text) + 1 : len;
	}
	return off;
}

static void assign_field(struct fm *fm, const char *k, const char *v)
{
	if (key_is(k, "title"))
		fm->title = v;
	else if (key_is(k, "date"))
		fm->date = v;
	else if (key_is(k, "tags"))
		fm->tags = v;
	else if (key_is(k, "img"))
		fm->img = v;
	else if (key_is(k, "description"))
		fm->description = v;
}

static void pick_fields(const strvec *keys, const strvec *vals, struct fm *fm)
{
	size_t i;

	for (i = 0; i < strvec_len(keys); i++)
		assign_field(fm, strvec_get(keys, i), strvec_get(vals, i));
}

static int require_date(const char *lang, const char *date)
{
	char formatted[128];

	if (!date || !date[0])
		return 0;
	if (!known_lang(lang))
		return -1;
	if (format_date_ymd(lang, date, formatted, sizeof formatted) != 0)
		return -1;
	return 0;
}

static void attr_date_raw(buf *b, const char *date);
static void attr_datefmt(buf *b, const char *lang, const char *date);

static void write_known(buf *b, const char *source, const char *lang, const struct fm *fm)
{
	if (source && source[0])
		attr(b, "data-sblg-source", source);
	if (fm->title)
		attr(b, "data-sblg-title", fm->title);
	attr_date_raw(b, fm->date);
	if (fm->tags)
		attr(b, "data-sblg-tags", fm->tags);
	if (fm->img)
		attr(b, "data-sblg-img", fm->img);
	attr_datefmt(b, lang, fm->date);
}

static void attr_date_raw(buf *b, const char *date)
{
	char raw[16];
	size_t n;

	if (!date)
		return;
	if (!date[0]) {
		attr(b, "data-sblg-datetime", "");
		return;
	}
	n = strlen(date) < 10 ? strlen(date) : 10;
	memcpy(raw, date, n);
	raw[n] = 0;
	attr(b, "data-sblg-datetime", raw);
}

static void attr_datefmt(buf *b, const char *lang, const char *date)
{
	char formatted[128];

	if (!date || !date[0])
		return;
	format_date_ymd(lang, date, formatted, sizeof formatted);
	attr(b, "data-sblg-set-datefmt", formatted);
}

static int reserved_key(const char *k)
{
	if (key_is(k, "title"))
		return 1;
	if (key_is(k, "date"))
		return 1;
	if (key_is(k, "tags"))
		return 1;
	if (key_is(k, "img"))
		return 1;
	if (key_is(k, "description"))
		return 1;
	return 0;
}

static void write_extra(buf *b, const strvec *keys, const strvec *vals)
{
	size_t i;

	for (i = 0; i < strvec_len(keys); i++) {
		const char *k = strvec_get(keys, i);
		char name[256];

		if (reserved_key(k))
			continue;
		snprintf(name, sizeof name, "data-sblg-set-%s", k);
		attr(b, name, strvec_get(vals, i));
	}
}

static void write_aside(buf *b, const char *description)
{
	char *esc;

	if (!description)
		return;
	esc = xml_escape(description);
	buf_str(b, "  <aside>");
	buf_str(b, esc);
	buf_str(b, "</aside>\n");
	free(esc);
}

int article_prelude(const char *text, size_t len, const char *source,
	const char *lang, char **prelude, size_t *body_off)
{
	strvec *keys = strvec_new();
	strvec *vals = strvec_new();
	struct fm fm = {0};
	buf b = {0};

	*prelude = NULL;
	*body_off = 0;
	if (lang_rejected(lang)) {
		strvec_free(keys);
		strvec_free(vals);
		return -1;
	}
	*body_off = front_matter(text, len, keys, vals);
	pick_fields(keys, vals, &fm);
	if (require_date(lang, fm.date) != 0) {
		strvec_free(keys);
		strvec_free(vals);
		return -1;
	}
	buf_str(&b, "<article data-sblg-article=\"1\"");
	write_known(&b, source, lang, &fm);
	write_extra(&b, keys, vals);
	buf_str(&b, ">\n");
	write_aside(&b, fm.description);
	*prelude = buf_take(&b);
	strvec_free(keys);
	strvec_free(vals);
	return 0;
}

static const char *tag_span_end(const char *tag, const char *close)
{
	const char *end = strstr(tag, close);

	if (!end)
		return NULL;
	if (memchr(tag, '<', (size_t)(end - tag)))
		return NULL;
	return end;
}

static void append_tag_link(buf *b, const char *tag, size_t tlen,
	const char *lang, const char *slug)
{
	buf_str(b, "<a href=\"/");
	buf_str(b, lang);
	buf_str(b, "/");
	buf_str(b, slug);
	buf_str(b, "/tag/");
	buf_add(b, tag, tlen);
	buf_str(b, ".html\" class=\"sblg-tag\">");
	buf_add(b, tag, tlen);
	buf_str(b, "</a>");
}

int linkify(const char *in, const char *lang, const char *slug, char **out)
{
	static const char open[] = "<span class=\"sblg-tag\">";
	static const char close[] = "</span>";
	const size_t olen = sizeof open - 1;
	const size_t clen = sizeof close - 1;
	buf b = {0};
	const char *p;
	int prev_tag = 0;

	if (!in || !lang || !slug)
		return -1;
	for (p = in; *p;) {
		const char *hit = strstr(p, open);
		const char *tag;
		const char *end;
		size_t gap;

		if (!hit) {
			buf_str(&b, p);
			break;
		}
		gap = (size_t)(hit - p);
		if (gap)
			buf_add(&b, p, gap);
		else if (prev_tag)
			buf_str(&b, " ");
		tag = hit + olen;
		end = tag_span_end(tag, close);
		if (!end) {
			buf_add(&b, hit, olen);
			p = hit + olen;
			prev_tag = 0;
			continue;
		}
		append_tag_link(&b, tag, (size_t)(end - tag), lang, slug);
		p = end + clen;
		prev_tag = 1;
	}
	*out = buf_take(&b);
	return 0;
}
