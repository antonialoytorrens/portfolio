#include "util.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

enum { MAXLANG = 16 };

typedef struct {
	char *author;
	char *base;
	char *langs_raw;
	char *srclang;
	char *year;
	char *content;
	char *i18n;
	char *templates;
	char *public_dir;
	char *work;
	char *sblg;
	char *lowdown;
	char *mkart;
	char *serve_host;
	char *serve_port;
	char *remote_user;
	char *remote_host;
	char *remote_path;
	char *rsync_opts;
	char *langv[MAXLANG];
	int nlang;
} cfg;

static cfg C;

typedef struct {
	char *lang;
	kvmap *map;
} cached;

static cached cache[MAXLANG];
static int ncache;

static char *env_or(const char *key, const char *def)
{
	const char *v = getenv(key);
	char *copy;

	if (!v || !v[0])
		v = def;
	copy = strdup(v);
	if (!copy)
		die("oom");
	return copy;
}

static void pathf(char *dst, size_t n, const char *fmt, ...)
{
	va_list ap;
	int k;

	va_start(ap, fmt);
	k = vsnprintf(dst, n, fmt, ap);
	va_end(ap);
	if (k < 0 || (size_t)k >= n)
		die("path too long");
}

static void load_cfg(void)
{
	char ybuf[16];
	char *p;
	char *save = NULL;
	const char *year_env = getenv("YEAR");

	memset(&C, 0, sizeof C);
	C.author = env_or("AUTHOR", "Antoni Aloy Torrens");
	C.base = env_or("BASE_DOMAIN", "https://antonialoytorrens.com");
	C.langs_raw = env_or("LANGS", "ca es en");
	C.srclang = env_or("SRCLANG", "ca");
	C.content = env_or("CONTENT_DIR", "content");
	C.i18n = env_or("I18N_DIR", "i18n");
	C.templates = env_or("TEMPLATE_DIR", "templates");
	C.public_dir = env_or("PUBLIC_DIR", "public");
	C.work = env_or("WORK_DIR", ".work");
	C.sblg = env_or("SBLG", "sblg");
	C.lowdown = env_or("LOWDOWN", "lowdown --html-no-skiphtml --html-no-escapehtml");
	C.mkart = env_or("MKART", "bin/mkarticle");
	C.serve_host = env_or("SERVE_HOST", "127.0.0.1");
	C.serve_port = env_or("SERVE_PORT", "8080");
	C.remote_user = env_or("REMOTE_USER", "user");
	C.remote_host = env_or("REMOTE_HOST", "example.com");
	C.remote_path = env_or("REMOTE_PATH", "/var/www/html");
	C.rsync_opts = env_or("RSYNC_OPTS", "-avz --delete");
	if (year_env && year_env[0])
		C.year = strdup(year_env);
	else {
		int y = year_from_time(time(NULL));

		if (y < 0)
			die("gmtime");
		snprintf(ybuf, sizeof ybuf, "%d", y);
		C.year = strdup(ybuf);
	}
	if (!C.year)
		die("oom");
	p = strdup(C.langs_raw);
	if (!p)
		die("oom");
	for (save = NULL, p = strtok_r(p, " \t", &save); p; p = strtok_r(NULL, " \t", &save)) {
		if (C.nlang == MAXLANG)
			die("too many languages");
		C.langv[C.nlang++] = strdup(p);
	}
}

static int lang_ok(const char *lang)
{
	int i;

	for (i = 0; i < C.nlang; i++) {
		if (strcmp(C.langv[i], lang) == 0)
			return 1;
	}
	return 0;
}

static kvmap *lang_map(const char *lang)
{
	char path[PATH_MAX];
	char *text;
	kvmap *m = NULL;
	int i;

	for (i = 0; i < ncache; i++) {
		if (strcmp(cache[i].lang, lang) == 0)
			return cache[i].map;
	}
	if (ncache == MAXLANG)
		die("too many languages");
	pathf(path, sizeof path, "%s/%s.json", C.i18n, lang);
	text = read_file(path, NULL);
	if (!text)
		die("read %s: %s", path, strerror(errno));
	if (json_load_mem(text, &m) != 0)
		die("json %s", path);
	free(text);
	cache[ncache].lang = strdup(lang);
	cache[ncache].map = m;
	ncache++;
	return m;
}

static const char *need(kvmap *m, const char *key)
{
	const char *v = kv_get(m, key);

	if (!v)
		die("missing key %s", key);
	return v;
}

static char *must_read(const char *path)
{
	char *text = read_file(path, NULL);

	if (!text)
		die("read %s: %s", path, strerror(errno));
	return text;
}

static void must_write(const char *path, const void *data, size_t len)
{
	if (write_file(path, data, len) != 0)
		die("write %s: %s", path, strerror(errno));
}

static void must_mkdir(const char *path)
{
	if (mkdir_p(path) != 0)
		die("mkdir %s: %s", path, strerror(errno));
}

static strvec *list_md(const char *dir)
{
	DIR *d = opendir(dir);
	struct dirent *de;
	strvec *v;

	if (!d)
		return NULL;
	v = strvec_new();
	while ((de = readdir(d))) {
		size_t n = strlen(de->d_name);

		if (n > 3 && strcmp(de->d_name + n - 3, ".md") == 0)
			strvec_push(v, de->d_name);
	}
	closedir(d);
	strvec_sort(v);
	return v;
}

static void stem(const char *fn, char *out, size_t n)
{
	size_t len = strlen(fn);

	if (len > 3 && strcmp(fn + len - 3, ".md") == 0)
		len -= 3;
	if (len >= n)
		die("name too long");
	memcpy(out, fn, len);
	out[len] = 0;
}

static void pagesrc(const char *lang, const char *name, char *out, size_t n)
{
	pathf(out, n, "%s/%s/%s.md", C.content, lang, name);
	if (access(out, R_OK) == 0)
		return;
	pathf(out, n, "%s/%s/%s.md", C.content, C.srclang, name);
}

static void selfurls(kvmap *page, const char *name)
{
	int i;

	for (i = 0; i < C.nlang; i++) {
		const char *x = C.langv[i];
		char key[64];
		char val[PATH_MAX];
		const char *slug = NULL;

		if (name) {
			char sk[64];

			snprintf(sk, sizeof sk, "slug.%s", name);
			slug = need(lang_map(x), sk);
			pathf(val, sizeof val, "/%s/%s/", x, slug);
		} else
			pathf(val, sizeof val, "/%s/", x);
		snprintf(key, sizeof key, "url.self.%s", x);
		if (kv_put(page, key, val) != 0)
			die("oom");
	}
}

static char *concat(const char *a, const char *b)
{
	size_t na = strlen(a);
	size_t nb = strlen(b);
	char *out = malloc(na + nb + 1);

	if (!out)
		die("oom");
	memcpy(out, a, na);
	memcpy(out + na, b, nb);
	out[na + nb] = 0;
	return out;
}

static kvmap *page_new(const char *section, const char *heading,
	const char *title, const char *desc, const char *name)
{
	kvmap *page;

	if (kv_new(&page) != 0)
		die("oom");
	if (kv_put(page, "section", section) != 0 ||
		kv_put(page, "page_heading", heading) != 0 ||
		kv_put(page, "page_title", title) != 0 ||
		kv_put(page, "page_description", desc) != 0)
		die("oom");
	selfurls(page, name);
	return page;
}

static void sblg_run(char **flags, size_t nflags, const char *out,
	const char *tmpl, char **files, size_t nfiles)
{
	strvec *av = strvec_new();
	size_t i;

	strvec_push(av, C.sblg);
	for (i = 0; i < nflags; i++)
		strvec_push(av, flags[i]);
	strvec_push(av, "-o");
	strvec_push(av, out);
	strvec_push(av, "-t");
	strvec_push(av, tmpl);
	for (i = 0; i < nfiles; i++)
		strvec_push(av, files[i]);
	if (strvec_spawn(av) != 0)
		die("sblg %s", out);
	strvec_free(av);
}

static char *assemble(const char *tpl)
{
	char head[PATH_MAX];
	char mid[PATH_MAX];
	char foot[PATH_MAX];
	char *a;
	char *b;
	char *c;
	char *ab;
	char *out;

	pathf(head, sizeof head, "%s/partials/head.html", C.templates);
	pathf(mid, sizeof mid, "%s/%s.html", C.templates, tpl);
	pathf(foot, sizeof foot, "%s/partials/foot.html", C.templates);
	a = must_read(head);
	b = must_read(mid);
	c = must_read(foot);
	ab = concat(a, b);
	free(a);
	free(b);
	out = concat(ab, c);
	free(ab);
	free(c);
	return out;
}

static void linkify_file(const char *path, const char *lang, const char *slug)
{
	char *in = must_read(path);
	char *out = NULL;
	char tmp[PATH_MAX];

	if (linkify(in, lang, slug, &out) != 0)
		die("linkify %s", path);
	pathf(tmp, sizeof tmp, "%s.tmp", path);
	must_write(tmp, out, strlen(out));
	if (rename(tmp, path) != 0)
		die("rename %s: %s", path, strerror(errno));
	free(in);
	free(out);
}

static void publish(const char *lang, const char *blogslug, const char *tpl,
	const char *work, const char *out, int dolink,
	char **flags, size_t nflags, char **files, size_t nfiles,
	const inject_item *inj, size_t ninj, kvmap *page)
{
	kvmap *map = NULL;
	char *raw;
	char *step;
	char *baked = NULL;

	if (kv_new(&map) != 0)
		die("oom");
	if (kv_merge(map, lang_map(lang)) != 0)
		die("oom");
	if (kv_put(map, "lang", lang) != 0 || kv_put(map, "author", C.author) != 0 ||
		kv_put(map, "year", C.year) != 0)
		die("oom");
	if (kv_merge(map, page) != 0)
		die("oom");

	raw = assemble(tpl);
	if (ninj) {
		if (inject_apply(raw, inj, ninj, &step) != 0)
			die("inject %s", tpl);
		free(raw);
	} else
		step = raw;
	if (subst_apply(step, map, &baked) != 0)
		die("subst %s", tpl);
	free(step);
	must_write(work, baked, strlen(baked));
	free(baked);

	sblg_run(flags, nflags, out, work, files, nfiles);
	kv_free(map);
	if (dolink)
		linkify_file(out, lang, blogslug);
}

static void run_mk(char **argv)
{
	if (spawn_wait(argv) != 0)
		die("mkarticle %s", argv[1]);
}

static void build_one(const char *lang)
{
	kvmap *jm;
	const char *sb;
	const char *se;
	const char *sp;
	const char *sx;
	char dir[PATH_MAX];
	char blogdir[PATH_MAX];
	strvec *posts;
	strvec *frags;
	size_t i;
	char intro[PATH_MAX];
	char src[PATH_MAX];
	char *html;
	size_t hlen;
	const char *th;
	const char *tb;
	const char *thead;
	kvmap *page;
	inject_item inj[2];
	char item[PATH_MAX];
	char **files;
	size_t nfiles;
	char work[PATH_MAX];
	char out[PATH_MAX];
	char *cflag[1];

	if (!lang_ok(lang))
		die("usage: make build-one LANG=ca|es|en");
	jm = lang_map(lang);
	sb = need(jm, "slug.blog");
	se = need(jm, "slug.education");
	sp = need(jm, "slug.projects");
	sx = need(jm, "slug.experience");
	printf("==> Building language '%s'\n", lang);

	pathf(dir, sizeof dir, "%s/%s/blog", C.work, lang);
	must_mkdir(dir);
	pathf(dir, sizeof dir, "%s/%s", C.public_dir, lang);
	must_mkdir(dir);

	pathf(blogdir, sizeof blogdir, "%s/%s/blog", C.content, lang);
	posts = list_md(blogdir);
	if (!posts || strvec_len(posts) == 0) {
		strvec_free(posts);
		pathf(blogdir, sizeof blogdir, "%s/%s/blog", C.content, C.srclang);
		posts = list_md(blogdir);
	}
	if (!posts)
		posts = strvec_new();

	frags = strvec_new();
	for (i = 0; i < strvec_len(posts); i++) {
		char base[256];
		char xml[PATH_MAX];
		char source[PATH_MAX];
		char md[PATH_MAX];
		char *args[9];

		stem(strvec_get(posts, i), base, sizeof base);
		pathf(md, sizeof md, "%s/%s", blogdir, strvec_get(posts, i));
		pathf(xml, sizeof xml, "%s/%s/blog/%s.xml", C.work, lang, base);
		pathf(source, sizeof source, "%s/post/%s.html", sb, base);
		args[0] = C.mkart;
		args[1] = md;
		args[2] = "--source";
		args[3] = source;
		args[4] = "--lang";
		args[5] = (char *)lang;
		args[6] = "-o";
		args[7] = xml;
		args[8] = NULL;
		run_mk(args);
		strvec_push(frags, xml);
	}

	pagesrc(lang, "index", src, sizeof src);
	pathf(intro, sizeof intro, "%s/%s/intro.html", C.work, lang);
	html = NULL;
	hlen = 0;
	if (run_cmd(C.lowdown, src, &html, &hlen) != 0)
		die("lowdown %s", src);
	must_write(intro, html, hlen);
	free(html);

	th = need(jm, "title.home");
	pathf(item, sizeof item, "%s/partials/postitem.html", C.templates);
	page = page_new("about", th, th, "", NULL);
	inj[0].key = "home_intro";
	inj[0].path = intro;
	inj[1].key = "post_item";
	inj[1].path = item;
	nfiles = strvec_len(frags);
	files = nfiles ? malloc(nfiles * sizeof *files) : NULL;
	for (i = 0; i < nfiles; i++)
		files[i] = strvec_get(frags, i);
	pathf(work, sizeof work, "%s/%s/tmpl-index.html", C.work, lang);
	pathf(out, sizeof out, "%s/%s/index.html", C.public_dir, lang);
	publish(lang, sb, "index", work, out, 1, NULL, 0, files, nfiles, inj, 2, page);
	kv_free(page);

	{
		const char *names[] = {"education", "projects", "experience"};
		const char *slugs[] = {se, sp, sx};
		size_t pi;

		cflag[0] = "-c";
		for (pi = 0; pi < 3; pi++) {
			char md[PATH_MAX];
			char xml[PATH_MAX];
			char *one[1];
			char *args[8];

			pathf(dir, sizeof dir, "%s/%s/%s", C.public_dir, lang, slugs[pi]);
			must_mkdir(dir);
			pagesrc(lang, names[pi], md, sizeof md);
			pathf(xml, sizeof xml, "%s/%s/page-%s.xml", C.work, lang, names[pi]);
			args[0] = C.mkart;
			args[1] = md;
			args[2] = "--lang";
			args[3] = (char *)lang;
			args[4] = "-o";
			args[5] = xml;
			args[6] = NULL;
			run_mk(args);
			page = page_new(names[pi], "${sblg-title}", "${sblg-titletext}",
				"${sblg-aside}", names[pi]);
			one[0] = xml;
			pathf(work, sizeof work, "%s/%s/tmpl-%s.html", C.work, lang, names[pi]);
			pathf(out, sizeof out, "%s/index.html", dir);
			publish(lang, sb, "page", work, out, 0, cflag, 1, one, 1, NULL, 0, page);
			kv_free(page);
		}
	}

	pathf(dir, sizeof dir, "%s/%s/%s", C.public_dir, lang, sb);
	must_mkdir(dir);
	tb = need(jm, "title.blog");
	page = page_new("blog", tb, tb, "", "blog");
	inj[0].key = "post_item";
	inj[0].path = item;
	pathf(work, sizeof work, "%s/%s/tmpl-blog.html", C.work, lang);
	pathf(out, sizeof out, "%s/index.html", dir);
	publish(lang, sb, "blog", work, out, 1, NULL, 0, files, nfiles, inj, 1, page);
	kv_free(page);

	pathf(dir, sizeof dir, "%s/%s/%s/post", C.public_dir, lang, sb);
	must_mkdir(dir);
	for (i = 0; i < nfiles; i++) {
		const char *frag = files[i];
		const char *base = strrchr(frag, '/');
		char stemname[256];
		char *one[1];
		size_t bl;

		base = base ? base + 1 : frag;
		bl = strlen(base);
		if (bl > 4 && strcmp(base + bl - 4, ".xml") == 0)
			bl -= 4;
		if (bl >= sizeof stemname)
			die("name too long");
		memcpy(stemname, base, bl);
		stemname[bl] = 0;
		page = page_new("blog", "${sblg-title}", "${sblg-titletext}",
			"${sblg-aside}", "blog");
		one[0] = files[i];
		cflag[0] = "-c";
		pathf(work, sizeof work, "%s/%s/tmpl-post.html", C.work, lang);
		pathf(out, sizeof out, "%s/%s.html", dir, stemname);
		publish(lang, sb, "post", work, out, 1, cflag, 1, one, 1, NULL, 0, page);
		kv_free(page);
	}

	pathf(dir, sizeof dir, "%s/%s/%s/tag", C.public_dir, lang, sb);
	must_mkdir(dir);
	thead = need(jm, "tag.heading");
	if (nfiles) {
		strvec *av = strvec_new();
		char *listed = NULL;
		strvec *tags = NULL;
		size_t t;

		strvec_push(av, C.sblg);
		strvec_push(av, "-l");
		for (i = 0; i < nfiles; i++)
			strvec_push(av, files[i]);
		if (strvec_capture(av, &listed, NULL) != 0)
			die("sblg -l");
		strvec_free(av);
		if (tags_collect(listed, &tags) != 0)
			die("tags");
		free(listed);
		for (t = 0; t < strvec_len(tags); t++) {
			const char *tag = strvec_get(tags, t);
			char title[512];

			snprintf(title, sizeof title, "%s %s", thead, tag);
			page = page_new("blog", tb, title, "", "blog");
			if (kv_put(page, "tag", tag) != 0)
				die("oom");
			inj[0].key = "post_item";
			inj[0].path = item;
			pathf(work, sizeof work, "%s/%s/tmpl-tag-%s.html", C.work, lang, tag);
			pathf(out, sizeof out, "%s/%s.html", dir, tag);
			publish(lang, sb, "tag", work, out, 1, NULL, 0, files, nfiles, inj, 1, page);
			kv_free(page);
		}
		strvec_free(tags);
	}

	{
		char atom_in[PATH_MAX];
		char atom_tmpl[PATH_MAX];
		char atom_out[PATH_MAX];
		char *raw;
		char *baked = NULL;
		char *aflag[1];

		pathf(atom_in, sizeof atom_in, "%s/atom.in.xml", C.templates);
		raw = must_read(atom_in);
		if (kv_new(&page) != 0)
			die("oom");
		if (kv_merge(page, jm) != 0)
			die("oom");
		if (kv_put(page, "lang", lang) != 0 || kv_put(page, "author", C.author) != 0 ||
			kv_put(page, "year", C.year) != 0 || kv_put(page, "base_url", C.base) != 0)
			die("oom");
		if (subst_apply(raw, page, &baked) != 0)
			die("subst atom");
		free(raw);
		kv_free(page);
		pathf(atom_tmpl, sizeof atom_tmpl, "%s/%s/atom-tmpl.xml", C.work, lang);
		must_write(atom_tmpl, baked, strlen(baked));
		free(baked);
		pathf(atom_out, sizeof atom_out, "%s/%s/atom.xml", C.public_dir, lang);
		aflag[0] = "-a";
		sblg_run(aflag, 1, atom_out, atom_tmpl, files, nfiles);
	}

	free(files);
	strvec_free(frags);
	strvec_free(posts);
	printf("    language '%s' done.\n", lang);
}

static void assets(void)
{
	char dest[PATH_MAX];
	char reset[PATH_MAX];
	char styles[PATH_MAX];
	char bundle[PATH_MAX];
	char *a;
	char *b;
	char *both;

	printf("==> Copying assets\n");
	must_mkdir(C.public_dir);
	pathf(dest, sizeof dest, "%s/assets", C.public_dir);
	if (rm_rf(dest) != 0)
		die("rm %s", dest);
	if (copy_tree("assets", dest) != 0)
		die("copy assets");
	pathf(reset, sizeof reset, "%s/css/reset.css", dest);
	pathf(styles, sizeof styles, "%s/css/styles.css", dest);
	pathf(bundle, sizeof bundle, "%s/css/bundle.css", dest);
	a = must_read(reset);
	b = must_read(styles);
	both = concat(a, b);
	must_write(bundle, both, strlen(both));
	free(a);
	free(b);
	free(both);
	if (unlink(reset) != 0 || unlink(styles) != 0)
		die("unlink css");
}

static void all(void)
{
	pid_t pids[MAXLANG];
	int i;
	int bad = 0;

	assets();
	fflush(stdout);
	for (i = 0; i < C.nlang; i++) {
		pid_t pid = fork();

		if (pid < 0)
			die("fork: %s", strerror(errno));
		if (pid == 0) {
			build_one(C.langv[i]);
			fflush(stdout);
			fflush(stderr);
			_exit(0);
		}
		pids[i] = pid;
	}
	for (i = 0; i < C.nlang; i++) {
		int st;

		if (waitpid(pids[i], &st, 0) < 0)
			bad = 1;
		else if (!WIFEXITED(st) || WEXITSTATUS(st) != 0)
			bad = 1;
	}
	if (bad)
		exit(1);
	printf("Build complete -> %s/\n", C.public_dir);
}

static void clean(void)
{
	printf("==> Cleaning %s/ and %s/\n", C.public_dir, C.work);
	if (rm_rf(C.public_dir) != 0 || rm_rf(C.work) != 0)
		die("clean");
}

static void serve(void)
{
	char *av[9];

	all();
	printf("==> Serving %s/ at http://%s:%s/\n", C.public_dir, C.serve_host, C.serve_port);
	av[0] = "mini_httpd";
	av[1] = "-D";
	av[2] = "-h";
	av[3] = C.serve_host;
	av[4] = "-p";
	av[5] = C.serve_port;
	av[6] = "-d";
	av[7] = C.public_dir;
	av[8] = NULL;
	if (spawn_wait(av) != 0)
		die("mini_httpd");
}

static void sync_site(void)
{
	char src[PATH_MAX];
	char dest[PATH_MAX];
	strvec *av;
	char *buf;
	char *save = NULL;
	char *tok;

	all();
	printf("==> Syncing %s/ to %s@%s:%s\n", C.public_dir, C.remote_user, C.remote_host, C.remote_path);
	pathf(src, sizeof src, "%s/", C.public_dir);
	pathf(dest, sizeof dest, "%s@%s:%s", C.remote_user, C.remote_host, C.remote_path);
	av = strvec_new();
	strvec_push(av, "rsync");
	buf = strdup(C.rsync_opts);
	if (!buf)
		die("oom");
	for (tok = strtok_r(buf, " \t", &save); tok; tok = strtok_r(NULL, " \t", &save))
		strvec_push(av, tok);
	free(buf);
	strvec_push(av, src);
	strvec_push(av, dest);
	if (strvec_spawn(av) != 0)
		die("rsync");
	strvec_free(av);
}

static void help(void)
{
	printf("Targets: all (default), assets, build-one LANG=<lang>, clean, serve, rsync\n");
	printf("Languages: %s  (source/fallback: %s)\n", C.langs_raw, C.srclang);
}

int main(int argc, char **argv)
{
	const char *cmd = argc > 1 ? argv[1] : "all";

	load_cfg();
	if (strcmp(cmd, "all") == 0)
		all();
	else if (strcmp(cmd, "assets") == 0)
		assets();
	else if (strcmp(cmd, "build-one") == 0) {
		const char *lang = getenv("LANG");

		if (!lang)
			die("usage: make build-one LANG=ca|es|en");
		build_one(lang);
	} else if (strcmp(cmd, "clean") == 0)
		clean();
	else if (strcmp(cmd, "serve") == 0)
		serve();
	else if (strcmp(cmd, "rsync") == 0)
		sync_site();
	else if (strcmp(cmd, "help") == 0)
		help();
	else
		die("unknown target: %s", cmd);
	return 0;
}
