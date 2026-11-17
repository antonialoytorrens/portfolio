#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "util.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include <cjson/cJSON.h>

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

struct kvmap {
	struct {
		char *key;
		char *value;
	} *tab;
};

struct strvec {
	char **v;
};

void die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}

static int cmpstr(const void *a, const void *b)
{
	const char *const *sa = a;
	const char *const *sb = b;

	return strcmp(*sa, *sb);
}

strvec *strvec_new(void)
{
	strvec *v = calloc(1, sizeof *v);

	if (!v)
		die("oom");
	return v;
}

void strvec_push(strvec *v, const char *s)
{
	char *copy = strdup(s);

	if (!copy)
		die("oom");
	arrput(v->v, copy);
}

size_t strvec_len(const strvec *v)
{
	if (!v || !v->v)
		return 0;
	return (size_t)arrlen(v->v);
}

char *strvec_get(const strvec *v, size_t i)
{
	return v->v[i];
}

void strvec_sort(strvec *v)
{
	size_t n = strvec_len(v);

	if (n > 1)
		qsort(v->v, n, sizeof v->v[0], cmpstr);
}

void strvec_free(strvec *v)
{
	ptrdiff_t i;

	if (!v)
		return;
	for (i = 0; i < arrlen(v->v); i++)
		free(v->v[i]);
	arrfree(v->v);
	free(v);
}

int kv_new(kvmap **out)
{
	kvmap *m = calloc(1, sizeof *m);

	if (!m)
		return -1;
	sh_new_strdup(m->tab);
	*out = m;
	return 0;
}

int kv_put(kvmap *m, const char *key, const char *value)
{
	char *copy = strdup(value);
	ptrdiff_t i;

	if (!copy)
		return -1;
	i = shgeti(m->tab, key);
	if (i >= 0) {
		free(m->tab[i].value);
		m->tab[i].value = copy;
		return 0;
	}
	shput(m->tab, key, copy);
	return 0;
}

const char *kv_get(kvmap *m, const char *key)
{
	ptrdiff_t i;

	if (!m || !m->tab)
		return NULL;
	i = shgeti(m->tab, key);
	if (i < 0)
		return NULL;
	return m->tab[i].value;
}

int kv_merge(kvmap *dst, kvmap *src)
{
	ptrdiff_t i;

	if (!src)
		return 0;
	for (i = 0; i < shlen(src->tab); i++) {
		if (kv_put(dst, src->tab[i].key, src->tab[i].value) != 0)
			return -1;
	}
	return 0;
}

void kv_free(kvmap *m)
{
	ptrdiff_t i;

	if (!m)
		return;
	for (i = 0; i < shlen(m->tab); i++)
		free(m->tab[i].value);
	shfree(m->tab);
	free(m);
}

int json_load_mem(const char *text, kvmap **out)
{
	cJSON *root;
	cJSON *it;
	kvmap *m = NULL;

	*out = NULL;
	if (!text)
		return -1;
	root = cJSON_Parse(text);
	if (!root || !cJSON_IsObject(root)) {
		cJSON_Delete(root);
		return -1;
	}
	if (kv_new(&m) != 0) {
		cJSON_Delete(root);
		return -1;
	}
	for (it = root->child; it; it = it->next) {
		if (!cJSON_IsString(it) || !it->string) {
			kv_free(m);
			cJSON_Delete(root);
			return -1;
		}
		if (kv_put(m, it->string, it->valuestring) != 0) {
			kv_free(m);
			cJSON_Delete(root);
			return -1;
		}
	}
	cJSON_Delete(root);
	*out = m;
	return 0;
}

typedef struct {
	char *s;
	size_t n;
	size_t cap;
} buf;

static void buf_add(buf *b, const void *p, size_t n)
{
	if (b->n + n + 1 < b->n)
		die("oom");
	if (b->n + n + 1 > b->cap) {
		size_t c = b->cap ? b->cap : 64;

		while (c < b->n + n + 1) {
			if (c > (size_t)-1 / 2)
				die("oom");
			c *= 2;
		}
		char *s = realloc(b->s, c);

		if (!s)
			die("oom");
		b->s = s;
		b->cap = c;
	}
	if (n)
		memcpy(b->s + b->n, p, n);
	b->n += n;
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

int subst_apply(const char *in, kvmap *m, char **out)
{
	buf b = {0};
	const char *p;

	if (!in)
		return -1;
	for (p = in; *p;) {
		const char *end;
		char *key;
		const char *val;

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
		val = kv_get(m, key);
		if (val)
			buf_str(&b, val);
		else {
			buf_str(&b, "@@");
			buf_str(&b, key);
			buf_str(&b, "@@");
		}
		free(key);
		p = end + 2;
	}
	*out = buf_take(&b);
	return 0;
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
		size_t i;
		int hit = 0;

		for (i = 0; i < n; i++) {
			char *pat;
			size_t plen;
			const char *hitp;
			char *file;
			size_t flen = 0;

			plen = strlen(items[i].key) + 4;
			pat = malloc(plen + 1);
			if (!pat)
				die("oom");
			snprintf(pat, plen + 1, "@@%s@@", items[i].key);
			hitp = NULL;
			if (linelen >= plen) {
				size_t k;

				for (k = 0; k + plen <= linelen; k++) {
					if (memcmp(p + k, pat, plen) == 0) {
						hitp = p + k;
						break;
					}
				}
			}
			free(pat);
			if (!hitp)
				continue;
			file = read_file(items[i].path, &flen);
			if (!file) {
				free(b.s);
				return -1;
			}
			buf_add(&b, file, flen);
			free(file);
			hit = 1;
			break;
		}
		if (!hit) {
			buf_add(&b, p, linelen);
			if (nl)
				buf_add(&b, "\n", 1);
		}
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

static int parse_ymd(const char *s, int *y, int *m, int *d)
{
	int i;

	if (!s || strlen(s) < 10)
		return -1;
	for (i = 0; i < 10; i++) {
		if (i == 4 || i == 7) {
			if (s[i] != '-')
				return -1;
		} else if (s[i] < '0' || s[i] > '9')
			return -1;
	}
	*y = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
	*m = (s[5] - '0') * 10 + (s[6] - '0');
	*d = (s[8] - '0') * 10 + (s[9] - '0');
	if (*d < 1 || *d > dim(*y, *m))
		return -1;
	return 0;
}

int format_date_ymd(const char *lang, const char *ymd, char *out, size_t outsz)
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
	const char *const *names;
	int y, m, d, n, kind;

	if (!lang || parse_ymd(ymd, &y, &m, &d) != 0)
		return -1;
	if (strcmp(lang, "ca") == 0)
		kind = 0;
	else if (strcmp(lang, "es") == 0)
		kind = 1;
	else if (strcmp(lang, "en") == 0)
		kind = 2;
	else
		return -1;
	names = kind == 0 ? ca : kind == 1 ? es : en;
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

int article_prelude(const char *text, size_t len, const char *source,
	const char *lang, char **prelude, size_t *body_off)
{
	strvec *keys = strvec_new();
	strvec *vals = strvec_new();
	size_t off = 0;
	int infm = 0;
	const char *title = NULL;
	const char *date = NULL;
	const char *tags = NULL;
	const char *img = NULL;
	const char *description = NULL;
	buf b = {0};
	size_t i;

	*prelude = NULL;
	*body_off = 0;
	if (lang && lang[0] && !known_lang(lang)) {
		strvec_free(keys);
		strvec_free(vals);
		return -1;
	}
	if (len >= 3 && is_fence(text, (size_t)(strchr(text, '\n') ? strchr(text, '\n') - text : (ptrdiff_t)len))) {
		const char *nl = memchr(text, '\n', len);

		off = nl ? (size_t)(nl - text) + 1 : len;
		infm = 1;
	}
	while (infm && off < len) {
		const char *line = text + off;
		const char *nl = memchr(line, '\n', len - off);
		size_t linelen = nl ? (size_t)(nl - line) : len - off;
		char *key;
		char *val;
		const char *colon;

		if (is_fence(line, linelen)) {
			off = nl ? (size_t)(nl - text) + 1 : len;
			infm = 0;
			break;
		}
		colon = memchr(line, ':', linelen);
		if (colon) {
			key = trim_dup(line, (size_t)(colon - line));
			val = trim_dup(colon + 1, linelen - (size_t)(colon + 1 - line));
			if (key[0]) {
				strvec_push(keys, key);
				strvec_push(vals, val);
			}
			free(key);
			free(val);
		}
		off = nl ? (size_t)(nl - text) + 1 : len;
	}
	*body_off = off;

	for (i = 0; i < strvec_len(keys); i++) {
		const char *k = strvec_get(keys, i);
		const char *v = strvec_get(vals, i);

		if (key_is(k, "title"))
			title = v;
		else if (key_is(k, "date"))
			date = v;
		else if (key_is(k, "tags"))
			tags = v;
		else if (key_is(k, "img"))
			img = v;
		else if (key_is(k, "description"))
			description = v;
	}
	if (date && date[0]) {
		char formatted[128];

		if (!known_lang(lang) || format_date_ymd(lang, date, formatted, sizeof formatted) != 0) {
			strvec_free(keys);
			strvec_free(vals);
			return -1;
		}
	}

	buf_str(&b, "<article data-sblg-article=\"1\"");
	if (source && source[0])
		attr(&b, "data-sblg-source", source);
	if (title)
		attr(&b, "data-sblg-title", title);
	if (date && date[0]) {
		char raw[16];
		size_t n = strlen(date) < 10 ? strlen(date) : 10;

		memcpy(raw, date, n);
		raw[n] = 0;
		attr(&b, "data-sblg-datetime", raw);
	} else if (date)
		attr(&b, "data-sblg-datetime", "");
	if (tags)
		attr(&b, "data-sblg-tags", tags);
	if (img)
		attr(&b, "data-sblg-img", img);
	if (date && date[0]) {
		char formatted[128];

		format_date_ymd(lang, date, formatted, sizeof formatted);
		attr(&b, "data-sblg-set-datefmt", formatted);
	}
	for (i = 0; i < strvec_len(keys); i++) {
		const char *k = strvec_get(keys, i);
		char name[256];

		if (key_is(k, "title") || key_is(k, "date") || key_is(k, "tags") ||
			key_is(k, "img") || key_is(k, "description"))
			continue;
		snprintf(name, sizeof name, "data-sblg-set-%s", k);
		attr(&b, name, strvec_get(vals, i));
	}
	buf_str(&b, ">\n");
	if (description) {
		char *esc = xml_escape(description);

		buf_str(&b, "  <aside>");
		buf_str(&b, esc);
		buf_str(&b, "</aside>\n");
		free(esc);
	}
	*prelude = buf_take(&b);
	strvec_free(keys);
	strvec_free(vals);
	return 0;
}

int linkify(const char *in, const char *lang, const char *slug, char **out)
{
	static const char open[] = "<span class=\"sblg-tag\">";
	static const char close[] = "</span>";
	const size_t olen = sizeof open - 1;
	const size_t clen = sizeof close - 1;
	buf b = {0};
	const char *p;

	if (!in || !lang || !slug)
		return -1;
	for (p = in; *p;) {
		const char *hit = strstr(p, open);
		const char *tag;
		const char *end;
		const char *lt;

		if (!hit) {
			buf_str(&b, p);
			break;
		}
		buf_add(&b, p, (size_t)(hit - p));
		tag = hit + olen;
		end = strstr(tag, close);
		lt = end ? memchr(tag, '<', (size_t)(end - tag)) : NULL;
		if (!end || lt) {
			buf_add(&b, hit, olen);
			p = hit + olen;
			continue;
		}
		buf_str(&b, "<a href=\"/");
		buf_str(&b, lang);
		buf_str(&b, "/");
		buf_str(&b, slug);
		buf_str(&b, "/tag/");
		buf_add(&b, tag, (size_t)(end - tag));
		buf_str(&b, ".html\" class=\"sblg-tag\">");
		buf_add(&b, tag, (size_t)(end - tag));
		buf_str(&b, "</a>");
		p = end + clen;
	}
	*out = buf_take(&b);
	return 0;
}

int tags_collect(const char *text, strvec **out)
{
	strvec *raw = strvec_new();
	strvec *uniq;
	const char *p;
	size_t i;

	if (!text)
		text = "";
	for (p = text; *p;) {
		const char *nl = strchr(p, '\n');
		size_t n = nl ? (size_t)(nl - p) : strlen(p);
		const char *tab;
		const char *field;
		size_t flen;
		char *s;

		if (n && p[n - 1] == '\r')
			n--;
		tab = memchr(p, '\t', n);
		if (!tab) {
			field = p;
			flen = n;
		} else {
			const char *tab2 = memchr(tab + 1, '\t', (size_t)(p + n - (tab + 1)));

			field = tab + 1;
			flen = tab2 ? (size_t)(tab2 - field) : (size_t)(p + n - field);
		}
		if (flen) {
			s = malloc(flen + 1);
			if (!s)
				die("oom");
			memcpy(s, field, flen);
			s[flen] = 0;
			arrput(raw->v, s);
		}
		p = nl ? nl + 1 : p + n;
	}
	strvec_sort(raw);
	uniq = strvec_new();
	for (i = 0; i < strvec_len(raw); i++) {
		if (i && strcmp(strvec_get(raw, i), strvec_get(raw, i - 1)) == 0)
			continue;
		strvec_push(uniq, strvec_get(raw, i));
	}
	strvec_free(raw);
	*out = uniq;
	return 0;
}

char *read_file(const char *path, size_t *len)
{
	FILE *fp = fopen(path, "rb");
	char *buf = NULL;
	size_t n = 0;
	size_t cap = 0;

	if (!fp)
		return NULL;
	for (;;) {
		if (n + 4096 + 1 < n) {
			free(buf);
			fclose(fp);
			return NULL;
		}
		if (n + 4096 + 1 > cap) {
			size_t c = cap ? cap * 2 : 4096;
			char *grown = realloc(buf, c);

			if (!grown) {
				free(buf);
				fclose(fp);
				return NULL;
			}
			buf = grown;
			cap = c;
		}
		size_t got = fread(buf + n, 1, 4096, fp);

		n += got;
		if (got < 4096) {
			if (ferror(fp)) {
				free(buf);
				fclose(fp);
				return NULL;
			}
			break;
		}
	}
	fclose(fp);
	if (!buf)
		buf = malloc(1);
	if (!buf)
		return NULL;
	buf[n] = 0;
	if (len)
		*len = n;
	return buf;
}

int write_full(int fd, const void *data, size_t len)
{
	const char *p = data;

	while (len) {
		ssize_t w = write(fd, p, len);

		if (w < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		p += w;
		len -= (size_t)w;
	}
	return 0;
}

int write_file(const char *path, const void *data, size_t len)
{
	FILE *fp = fopen(path, "wb");

	if (!fp)
		return -1;
	if (len && fwrite(data, 1, len, fp) != len) {
		fclose(fp);
		return -1;
	}
	if (fclose(fp) != 0)
		return -1;
	return 0;
}

int mkdir_p(const char *path)
{
	char *tmp = strdup(path);
	char *p;

	if (!tmp)
		return -1;
	for (p = tmp + 1; *p; p++) {
		if (*p != '/')
			continue;
		*p = 0;
		if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
			free(tmp);
			return -1;
		}
		*p = '/';
	}
	if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
		free(tmp);
		return -1;
	}
	free(tmp);
	return 0;
}

static int copy_one(const char *src, const char *dst)
{
	struct stat st;
	char target[4096];
	ssize_t n;
	FILE *in;
	FILE *out;
	char blk[8192];
	size_t got;

	if (lstat(src, &st) != 0)
		return -1;
	if (S_ISLNK(st.st_mode)) {
		n = readlink(src, target, sizeof target - 1);
		if (n < 0)
			return -1;
		target[n] = 0;
		unlink(dst);
		return symlink(target, dst) == 0 ? 0 : -1;
	}
	in = fopen(src, "rb");
	if (!in)
		return -1;
	out = fopen(dst, "wb");
	if (!out) {
		fclose(in);
		return -1;
	}
	while ((got = fread(blk, 1, sizeof blk, in)) > 0) {
		if (fwrite(blk, 1, got, out) != got) {
			fclose(in);
			fclose(out);
			return -1;
		}
	}
	if (ferror(in)) {
		fclose(in);
		fclose(out);
		return -1;
	}
	fclose(in);
	if (fchmod(fileno(out), st.st_mode & 0777) != 0) {
		fclose(out);
		return -1;
	}
	if (fclose(out) != 0)
		return -1;
	return 0;
}

int copy_tree(const char *src, const char *dst)
{
	struct stat st;
	DIR *d;
	struct dirent *de;

	if (lstat(src, &st) != 0)
		return -1;
	if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode))
		return copy_one(src, dst);
	if (!S_ISDIR(st.st_mode))
		return -1;
	if (mkdir_p(dst) != 0)
		return -1;
	d = opendir(src);
	if (!d)
		return -1;
	while ((de = readdir(d))) {
		char from[4096];
		char to[4096];

		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;
		if (snprintf(from, sizeof from, "%s/%s", src, de->d_name) >= (int)sizeof from ||
			snprintf(to, sizeof to, "%s/%s", dst, de->d_name) >= (int)sizeof to) {
			closedir(d);
			return -1;
		}
		if (copy_tree(from, to) != 0) {
			closedir(d);
			return -1;
		}
	}
	closedir(d);
	return 0;
}

int rm_rf(const char *path)
{
	struct stat st;
	DIR *d;
	struct dirent *de;

	if (lstat(path, &st) != 0)
		return errno == ENOENT ? 0 : -1;
	if (S_ISDIR(st.st_mode)) {
		d = opendir(path);
		if (!d)
			return -1;
		while ((de = readdir(d))) {
			char child[4096];

			if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
				continue;
			if (snprintf(child, sizeof child, "%s/%s", path, de->d_name) >= (int)sizeof child) {
				closedir(d);
				return -1;
			}
			if (rm_rf(child) != 0) {
				closedir(d);
				return -1;
			}
		}
		closedir(d);
		return rmdir(path) == 0 ? 0 : -1;
	}
	return unlink(path) == 0 ? 0 : -1;
}

int spawn_wait(char *const argv[])
{
	pid_t pid;
	int st;

	pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		execvp(argv[0], argv);
		fprintf(stderr, "exec %s: %s\n", argv[0], strerror(errno));
		_exit(127);
	}
	if (waitpid(pid, &st, 0) < 0)
		return -1;
	if (!WIFEXITED(st) || WEXITSTATUS(st) != 0)
		return -1;
	return 0;
}

int spawn_capture(char *const argv[], char **out, size_t *outlen)
{
	int pfd[2];
	pid_t pid;
	int st;
	char *buf = NULL;
	size_t n = 0;
	size_t cap = 0;

	if (pipe(pfd) != 0)
		return -1;
	pid = fork();
	if (pid < 0) {
		close(pfd[0]);
		close(pfd[1]);
		return -1;
	}
	if (pid == 0) {
		dup2(pfd[1], STDOUT_FILENO);
		close(pfd[0]);
		close(pfd[1]);
		execvp(argv[0], argv);
		fprintf(stderr, "exec %s: %s\n", argv[0], strerror(errno));
		_exit(127);
	}
	close(pfd[1]);
	for (;;) {
		ssize_t r;

		if (n + 4096 + 1 < n || n + 4096 + 1 > cap) {
			size_t c = cap ? cap * 2 : 4096;
			char *grown;

			if (c < n + 4096 + 1)
				c = n + 4096 + 1;
			grown = realloc(buf, c);
			if (!grown) {
				free(buf);
				close(pfd[0]);
				waitpid(pid, &st, 0);
				return -1;
			}
			buf = grown;
			cap = c;
		}
		r = read(pfd[0], buf + n, 4096);
		if (r < 0) {
			if (errno == EINTR)
				continue;
			free(buf);
			close(pfd[0]);
			waitpid(pid, &st, 0);
			return -1;
		}
		if (r == 0)
			break;
		n += (size_t)r;
	}
	close(pfd[0]);
	if (waitpid(pid, &st, 0) < 0) {
		free(buf);
		return -1;
	}
	if (!buf)
		buf = malloc(1);
	if (!buf)
		return -1;
	buf[n] = 0;
	*out = buf;
	if (outlen)
		*outlen = n;
	if (!WIFEXITED(st) || WEXITSTATUS(st) != 0)
		return -1;
	return 0;
}

static int spawn_vec(const strvec *v, char **out, size_t *outlen)
{
	size_t n = strvec_len(v);
	char **av = malloc((n + 1) * sizeof *av);
	size_t i;
	int rc;

	if (!av)
		return -1;
	for (i = 0; i < n; i++)
		av[i] = v->v[i];
	av[n] = NULL;
	if (out)
		rc = spawn_capture(av, out, outlen);
	else
		rc = spawn_wait(av);
	free(av);
	return rc;
}

int strvec_spawn(const strvec *v)
{
	return spawn_vec(v, NULL, NULL);
}

int strvec_capture(const strvec *v, char **out, size_t *outlen)
{
	return spawn_vec(v, out, outlen);
}

int run_cmd(const char *cmdline, const char *extra, char **out, size_t *outlen)
{
	char *buf;
	char *save = NULL;
	char *tok;
	char **av = NULL;
	size_t n = 0;
	size_t cap = 0;
	int rc;

	if (!cmdline)
		return -1;
	buf = strdup(cmdline);
	if (!buf)
		return -1;
	for (tok = strtok_r(buf, " \t", &save); tok; tok = strtok_r(NULL, " \t", &save)) {
		if (n + 3 > cap) {
			size_t c = cap ? cap * 2 : 8;
			char **grown = realloc(av, c * sizeof *av);

			if (!grown) {
				free(av);
				free(buf);
				return -1;
			}
			av = grown;
			cap = c;
		}
		av[n++] = tok;
	}
	if (extra) {
		if (n + 2 > cap) {
			char **grown = realloc(av, (n + 2) * sizeof *av);

			if (!grown) {
				free(av);
				free(buf);
				return -1;
			}
			av = grown;
		}
		av[n++] = (char *)extra;
	}
	if (!av) {
		free(buf);
		return -1;
	}
	av[n] = NULL;
	if (out)
		rc = spawn_capture(av, out, outlen);
	else
		rc = spawn_wait(av);
	free(av);
	free(buf);
	return rc;
}
