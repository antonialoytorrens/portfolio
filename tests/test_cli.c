#include "unity.h"
#include "util.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

enum { ENV_MAX = 32, ENV_LEN = 768 };
enum { POSTS_NONE = 0, POSTS_CA = 1, POSTS_FALLBACK = 2 };
enum { AUTHOR_OMIT = 0, AUTHOR_SET = 1, AUTHOR_EMPTY = 2 };

static char repo[4096];
static char build_bin[4096];
static char mk_bin[4096];

static const char json_ok[] =
	"{\n"
	"  \"slug.blog\": \"blog\",\n"
	"  \"slug.education\": \"education\",\n"
	"  \"slug.projects\": \"projects\",\n"
	"  \"slug.experience\": \"experience\",\n"
	"  \"title.home\": \"Home\",\n"
	"  \"title.blog\": \"Blog\",\n"
	"  \"tag.heading\": \"Tag\"\n"
	"}\n";

static const char json_nokey[] =
	"{\n"
	"  \"slug.blog\": \"blog\",\n"
	"  \"slug.education\": \"education\",\n"
	"  \"slug.projects\": \"projects\",\n"
	"  \"slug.experience\": \"experience\",\n"
	"  \"title.blog\": \"Blog\",\n"
	"  \"tag.heading\": \"Tag\"\n"
	"}\n";

static const char sblg_ok[] =
	"#!/bin/sh\n"
	"out=\n"
	"list=0\n"
	"prev=\n"
	"for a in \"$@\"; do\n"
	"\tif [ \"$prev\" = \"-o\" ]; then out=$a; fi\n"
	"\tif [ \"$a\" = \"-l\" ]; then list=1; fi\n"
	"\tprev=$a\n"
	"done\n"
	"if [ \"$list\" = 1 ]; then\n"
	"\tprintf \"x\\tweb\\ty\\n\"\n"
	"\texit 0\n"
	"fi\n"
	"if [ -n \"$out\" ]; then\n"
	"\tprintf '<p>ok</p>\\n' > \"$out\"\n"
	"fi\n"
	"exit 0\n";

static const char lowdown_ok[] =
	"#!/bin/sh\n"
	"printf '<p>hi</p>\\n'\n"
	"exit 0\n";

static const char mkart_ok[] =
	"#!/bin/sh\n"
	"out=\n"
	"prev=\n"
	"for a in \"$@\"; do\n"
	"\tif [ \"$prev\" = \"-o\" ]; then out=$a; fi\n"
	"\tprev=$a\n"
	"done\n"
	"printf '<article data-sblg-article=\"1\"></article>\\n' > \"$out\"\n"
	"exit 0\n";

static const char stub_ok[] = "#!/bin/sh\nexit 0\n";
static const char stub_fail[] = "#!/bin/sh\nexit 1\n";

static const char sblg_list_fail[] =
	"#!/bin/sh\n"
	"out=\n"
	"list=0\n"
	"prev=\n"
	"for a in \"$@\"; do\n"
	"\tif [ \"$prev\" = \"-o\" ]; then out=$a; fi\n"
	"\tif [ \"$a\" = \"-l\" ]; then list=1; fi\n"
	"\tprev=$a\n"
	"done\n"
	"if [ \"$list\" = 1 ]; then\n"
	"\texit 1\n"
	"fi\n"
	"if [ -n \"$out\" ]; then\n"
	"\tprintf '<p>ok</p>\\n' > \"$out\"\n"
	"fi\n"
	"exit 0\n";

static const char md_ok[] =
	"---\n"
	"title: Hi\n"
	"date: 2024-01-05\n"
	"---\n"
	"body\n";

struct opt {
	const char *langs;
	const char *srclang;
	const char *lang;
	const char *year;
	const char *author;
	int author_mode;
	int full;
	int lowdown_mode;
};

void setUp(void)
{
	int k;

	if (repo[0])
		return;
	TEST_ASSERT_NOT_NULL(getcwd(repo, sizeof repo));
	k = snprintf(build_bin, sizeof build_bin, "%s/bin/build-cov", repo);
	TEST_ASSERT_TRUE(k > 0 && (size_t)k < sizeof build_bin);
	k = snprintf(mk_bin, sizeof mk_bin, "%s/bin/mkarticle-cov", repo);
	TEST_ASSERT_TRUE(k > 0 && (size_t)k < sizeof mk_bin);
}

void tearDown(void) {}

static struct opt opt_site(const char *langs, const char *srclang, const char *lang)
{
	struct opt o;

	memset(&o, 0, sizeof o);
	o.full = 1;
	o.langs = langs;
	o.srclang = srclang;
	o.lang = lang;
	o.year = "2026";
	o.author_mode = AUTHOR_SET;
	o.author = "Test";
	return o;
}

static struct opt opt_min(int author_mode, const char *year)
{
	struct opt o;

	memset(&o, 0, sizeof o);
	o.author_mode = author_mode;
	o.author = "Test";
	o.year = year;
	return o;
}

static void put_text(const char *path, const char *text)
{
	TEST_ASSERT_EQUAL_INT(0, write_file(path, text, strlen(text)));
}

static void put_exe(const char *path, const char *text)
{
	put_text(path, text);
	TEST_ASSERT_EQUAL_INT(0, chmod(path, 0755));
}

static void rel(const char *root, const char *name, char *dst, size_t n)
{
	int k = snprintf(dst, n, "%s/%s", root, name);

	TEST_ASSERT_TRUE(k > 0 && (size_t)k < n);
}

static void lay(const char *root, const char *name, const char *text, int exe)
{
	char path[4096];

	rel(root, name, path, sizeof path);
	if (exe)
		put_exe(path, text);
	else
		put_text(path, text);
}

static char *new_root(void)
{
	char tmpl[] = "/tmp/portfolio-cliXXXXXX";
	char *made = mkdtemp(tmpl);

	TEST_ASSERT_NOT_NULL(made);
	return strdup(made);
}

static void layout(const char *root, int posts, int head_en, const char *ca_json)
{
	char path[4096];

	rel(root, "bin", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(0, mkdir_p(path));
	rel(root, "assets/css/vendor", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(0, mkdir_p(path));
	rel(root, "i18n", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(0, mkdir_p(path));
	rel(root, "templates/partials", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(0, mkdir_p(path));
	rel(root, "content/ca", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(0, mkdir_p(path));
	if (posts == POSTS_CA || posts == POSTS_FALLBACK) {
		rel(root, "content/ca/blog", path, sizeof path);
		TEST_ASSERT_EQUAL_INT(0, mkdir_p(path));
	}
	if (posts == POSTS_FALLBACK) {
		rel(root, "content/es/blog", path, sizeof path);
		TEST_ASSERT_EQUAL_INT(0, mkdir_p(path));
		lay(root, "content/es/blog/notes.txt", "no\n", 0);
		lay(root, "i18n/es.json", json_ok, 0);
	}

	lay(root, "bin/sblg", sblg_ok, 1);
	lay(root, "bin/lowdown", lowdown_ok, 1);
	lay(root, "bin/mkarticle", mkart_ok, 1);
	lay(root, "bin/mini_httpd", stub_ok, 1);
	lay(root, "bin/rsync", stub_ok, 1);
	lay(root, "assets/css/vendor/reset.css", "a\n", 0);
	lay(root, "assets/css/styles.css", "b\n", 0);
	lay(root, "templates/partials/head.html",
		head_en ? "<html lang=\"en\">\n" : "<html lang=\"es\">\n", 0);
	lay(root, "templates/partials/foot.html", "</html>\n", 0);
	lay(root, "templates/partials/postitem.html", "<li>item</li>\n", 0);
	lay(root, "templates/index.html", "@@home_intro@@\n@@post_item@@\n", 0);
	lay(root, "templates/page.html", "page\n", 0);
	lay(root, "templates/blog.html", "@@post_item@@\n", 0);
	lay(root, "templates/post.html", "post\n", 0);
	lay(root, "templates/tag.html", "@@post_item@@\n", 0);
	lay(root, "templates/atom.in.xml", "<feed>@@lang@@</feed>\n", 0);
	lay(root, "content/ca/index.md", "hello\n", 0);
	lay(root, "content/ca/education.md", "hello\n", 0);
	lay(root, "content/ca/projects.md", "hello\n", 0);
	lay(root, "content/ca/experience.md", "hello\n", 0);
	if (posts == POSTS_CA || posts == POSTS_FALLBACK) {
		lay(root, "content/ca/blog/hola.md", "hello\n", 0);
		lay(root, "content/ca/blog/notes.txt", "no\n", 0);
	}
	if (ca_json)
		lay(root, "i18n/ca.json", ca_json, 0);
}

static void env_add(char store[][ENV_LEN], char **envp, int *n, const char *fmt, ...)
{
	va_list ap;
	int k;

	TEST_ASSERT_TRUE(*n < ENV_MAX);
	va_start(ap, fmt);
	k = vsnprintf(store[*n], ENV_LEN, fmt, ap);
	va_end(ap);
	TEST_ASSERT_TRUE(k >= 0 && k < ENV_LEN);
	envp[*n] = store[*n];
	(*n)++;
}

static void dump_err(const char *root)
{
	char path[4096];
	char *text;

	rel(root, "err.txt", path, sizeof path);
	text = read_file(path, NULL);
	if (!text)
		return;
	fprintf(stderr, "%s\n", text);
	free(text);
}

static char *slurp_fd(int fd)
{
	char *buf = malloc(65536);
	size_t n = 0;
	ssize_t r;

	TEST_ASSERT_NOT_NULL(buf);
	while (n + 1 < 65536 && (r = read(fd, buf + n, 65536 - 1 - n)) > 0)
		n += (size_t)r;
	buf[n] = 0;
	return buf;
}

static int run_at(const char *root, char *const av[], const struct opt *o, char **captured)
{
	char store[ENV_MAX][ENV_LEN];
	char *envp[ENV_MAX + 1];
	int n = 0;
	int pfd[2];
	pid_t pid;
	int st = 0;

	{
		const char *prof = getenv("LLVM_PROFILE_FILE");

		if (prof && prof[0])
			env_add(store, envp, &n, "LLVM_PROFILE_FILE=%s", prof);
	}
	env_add(store, envp, &n, "PATH=%s/bin:/usr/bin:/bin", root);
	env_add(store, envp, &n, "PUBLIC_DIR=%s/public", root);
	env_add(store, envp, &n, "WORK_DIR=%s/work", root);
	if (o->author_mode == AUTHOR_EMPTY)
		env_add(store, envp, &n, "AUTHOR=");
	else if (o->author_mode == AUTHOR_SET)
		env_add(store, envp, &n, "AUTHOR=%s", o->author);
	if (o->year)
		env_add(store, envp, &n, "YEAR=%s", o->year);
	if (o->full) {
		env_add(store, envp, &n, "BASE_DOMAIN=https://example.test");
		if (o->langs)
			env_add(store, envp, &n, "LANGS=%s", o->langs);
		if (o->srclang)
			env_add(store, envp, &n, "SRCLANG=%s", o->srclang);
		env_add(store, envp, &n, "CONTENT_DIR=%s/content", root);
		env_add(store, envp, &n, "I18N_DIR=%s/i18n", root);
		env_add(store, envp, &n, "TEMPLATE_DIR=%s/templates", root);
		env_add(store, envp, &n, "SBLG=%s/bin/sblg", root);
		if (o->lowdown_mode == 2)
			env_add(store, envp, &n, "LOWDOWN=");
		else if (o->lowdown_mode == 0)
			env_add(store, envp, &n, "LOWDOWN=%s/bin/lowdown", root);
		env_add(store, envp, &n, "MKART=%s/bin/mkarticle", root);
		env_add(store, envp, &n, "SERVE_HOST=127.0.0.1");
		env_add(store, envp, &n, "SERVE_PORT=9");
		env_add(store, envp, &n, "REMOTE_USER=u");
		env_add(store, envp, &n, "REMOTE_HOST=h");
		env_add(store, envp, &n, "REMOTE_PATH=/var/www");
		env_add(store, envp, &n, "RSYNC_OPTS=-a");
		if (o->lang)
			env_add(store, envp, &n, "LANG=%s", o->lang);
	}
	envp[n] = NULL;

	if (captured) {
		TEST_ASSERT_EQUAL_INT(0, pipe(pfd));
		*captured = NULL;
	}
	fflush(NULL);
	pid = fork();
	TEST_ASSERT_TRUE(pid >= 0);
	if (pid == 0) {
		char err[4096];

		if (chdir(root) != 0)
			_exit(127);
		if (captured) {
			dup2(pfd[1], STDOUT_FILENO);
			close(pfd[0]);
			close(pfd[1]);
		} else {
			int dev = open("/dev/null", O_WRONLY);

			if (dev >= 0) {
				dup2(dev, STDOUT_FILENO);
				close(dev);
			}
		}
		if (snprintf(err, sizeof err, "%s/err.txt", root) < (int)sizeof err) {
			int efd = open(err, O_WRONLY | O_CREAT | O_TRUNC, 0644);

			if (efd >= 0) {
				dup2(efd, STDERR_FILENO);
				close(efd);
			}
		}
		execve(av[0], av, envp);
		_exit(127);
	}
	if (captured) {
		close(pfd[1]);
		*captured = slurp_fd(pfd[0]);
		close(pfd[0]);
	}
	TEST_ASSERT_TRUE(waitpid(pid, &st, 0) > 0);
	if (!WIFEXITED(st))
		return -1;
	return WEXITSTATUS(st);
}

static void expect(const char *root, char *const av[], struct opt o, int code)
{
	int st = run_at(root, av, &o, NULL);

	if (st != code)
		dump_err(root);
	TEST_ASSERT_EQUAL_INT(code, st);
}

static void test_build_flow(void)
{
	char *root = new_root();
	char *help[] = {build_bin, "help", NULL};
	char *unknown[] = {build_bin, "nope", NULL};
	char *assets[] = {build_bin, "assets", NULL};
	char *one[] = {build_bin, "build-one", NULL};
	char *clean[] = {build_bin, "clean", NULL};
	char *all[] = {build_bin, "all", NULL};
	char *serve[] = {build_bin, "serve", NULL};
	char *rsync[] = {build_bin, "rsync", NULL};
	char path[4096];
	char *text;

	layout(root, POSTS_CA, 1, json_ok);
	expect(root, help, opt_min(AUTHOR_EMPTY, NULL), 0);
	expect(root, unknown, opt_min(AUTHOR_OMIT, ""), 1);
	expect(root, assets, opt_site("ca", "ca", NULL), 0);
	rel(root, "public/assets/css/bundle.css", path, sizeof path);
	text = read_file(path, NULL);
	TEST_ASSERT_EQUAL_STRING("a\nb\n", text);
	free(text);
	rel(root, "public/assets/css/vendor/reset.css", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(-1, access(path, F_OK));
	rel(root, "public/assets/css/vendor", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(-1, access(path, F_OK));

	expect(root, one, opt_site("ca", "ca", "ca"), 0);
	rel(root, "public/ca/index.html", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(0, access(path, F_OK));
	rel(root, "public/ca/blog/post/hola.html", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(0, access(path, F_OK));
	rel(root, "public/ca/blog/tag/web.html", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(0, access(path, F_OK));
	rel(root, "public/ca/atom.xml", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(0, access(path, F_OK));
	rel(root, "public/ca/education/index.html", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(0, access(path, F_OK));

	expect(root, clean, opt_site("ca", "ca", NULL), 0);
	rel(root, "public", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(-1, access(path, F_OK));
	expect(root, all, opt_site("ca", "ca", NULL), 0);
	expect(root, serve, opt_site("ca", "ca", NULL), 0);
	expect(root, rsync, opt_site("ca", "ca", NULL), 0);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);
}

static void test_build_variants(void)
{
	char *root = new_root();
	char *one[] = {build_bin, "build-one", NULL};
	char path[4096];

	layout(root, POSTS_FALLBACK, 1, json_ok);
	expect(root, one, opt_site("es", "ca", "es"), 0);
	rel(root, "public/es/index.html", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(0, access(path, F_OK));
	rel(root, "public/es/blog/post/hola.html", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(0, access(path, F_OK));
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);

	root = new_root();
	layout(root, POSTS_NONE, 0, json_ok);
	expect(root, one, opt_site("ca", "ca", "ca"), 0);
	rel(root, "public/ca/index.html", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(0, access(path, F_OK));
	rel(root, "public/ca/blog/tag/web.html", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(-1, access(path, F_OK));
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);
}

static void test_build_early_errors(void)
{
	char *root = new_root();
	char *one[] = {build_bin, "build-one", NULL};
	char *all[] = {build_bin, "all", NULL};
	char langs[256];
	int i;
	int n = 0;

	layout(root, POSTS_CA, 1, json_ok);
	expect(root, one, opt_site("ca", "ca", NULL), 1);
	expect(root, one, opt_site("ca", "ca", "xx"), 1);
	for (i = 0; i < 17; i++) {
		int k = snprintf(langs + n, sizeof langs - (size_t)n, "%sl%d", i ? " " : "", i);

		TEST_ASSERT_TRUE(k > 0 && (size_t)n + (size_t)k < sizeof langs);
		n += k;
	}
	expect(root, all, opt_site(langs, "ca", NULL), 1);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);
}

static void test_build_input_errors(void)
{
	char *root;
	char *one[] = {build_bin, "build-one", NULL};
	char *assets[] = {build_bin, "assets", NULL};
	char path[4096];

	root = new_root();
	layout(root, POSTS_CA, 1, NULL);
	expect(root, one, opt_site("ca", "ca", "ca"), 1);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);

	root = new_root();
	layout(root, POSTS_CA, 1, "{");
	expect(root, one, opt_site("ca", "ca", "ca"), 1);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);

	root = new_root();
	layout(root, POSTS_CA, 1, json_nokey);
	expect(root, one, opt_site("ca", "ca", "ca"), 1);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);

	root = new_root();
	layout(root, POSTS_CA, 1, json_ok);
	rel(root, "templates/partials/head.html", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(0, unlink(path));
	expect(root, one, opt_site("ca", "ca", "ca"), 1);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);

	root = new_root();
	layout(root, POSTS_CA, 1, json_ok);
	rel(root, "assets", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(path));
	expect(root, assets, opt_site("ca", "ca", NULL), 1);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);
}

static void test_build_stub_errors(void)
{
	char *root;
	char *one[] = {build_bin, "build-one", NULL};
	char *all[] = {build_bin, "all", NULL};
	char path[4096];
	struct opt site = opt_site("ca", "ca", "ca");

	root = new_root();
	layout(root, POSTS_CA, 1, json_ok);
	rel(root, "bin/sblg", path, sizeof path);
	put_exe(path, stub_fail);
	expect(root, one, site, 1);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);

	root = new_root();
	layout(root, POSTS_CA, 1, json_ok);
	rel(root, "bin/lowdown", path, sizeof path);
	put_exe(path, stub_fail);
	expect(root, one, site, 1);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);

	root = new_root();
	layout(root, POSTS_CA, 1, json_ok);
	rel(root, "bin/mkarticle", path, sizeof path);
	put_exe(path, stub_fail);
	expect(root, one, site, 1);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);

	root = new_root();
	layout(root, POSTS_CA, 1, json_ok);
	rel(root, "bin/sblg", path, sizeof path);
	put_exe(path, stub_fail);
	expect(root, all, opt_site("ca", "ca", NULL), 1);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);
}

static void test_build_late_errors(void)
{
	char *root;
	char *one[] = {build_bin, "build-one", NULL};
	char *serve[] = {build_bin, "serve", NULL};
	char *rsync[] = {build_bin, "rsync", NULL};
	char *clean[] = {build_bin, "clean", NULL};
	char path[4096];
	struct opt site = opt_site("ca", "ca", "ca");

	root = new_root();
	layout(root, POSTS_CA, 1, json_ok);
	rel(root, "bin/sblg", path, sizeof path);
	put_exe(path, sblg_list_fail);
	expect(root, one, site, 1);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);

	root = new_root();
	layout(root, POSTS_CA, 1, json_ok);
	rel(root, "templates/partials/postitem.html", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(0, unlink(path));
	expect(root, one, site, 1);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);

	root = new_root();
	layout(root, POSTS_CA, 1, json_ok);
	rel(root, "bin/mini_httpd", path, sizeof path);
	put_exe(path, stub_fail);
	expect(root, serve, opt_site("ca", "ca", NULL), 1);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);

	root = new_root();
	layout(root, POSTS_CA, 1, json_ok);
	rel(root, "bin/rsync", path, sizeof path);
	put_exe(path, stub_fail);
	expect(root, rsync, opt_site("ca", "ca", NULL), 1);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);

	root = new_root();
	layout(root, POSTS_NONE, 1, json_ok);
	rel(root, "public/locked", path, sizeof path);
	TEST_ASSERT_EQUAL_INT(0, mkdir_p(path));
	TEST_ASSERT_EQUAL_INT(0, chmod(path, 0));
	expect(root, clean, opt_site("ca", "ca", NULL), 1);
	TEST_ASSERT_EQUAL_INT(0, chmod(path, 0755));
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);

	root = new_root();
	layout(root, POSTS_NONE, 1, json_ok);
	rel(root, "public", path, sizeof path);
	put_text(path, "x");
	expect(root, one, site, 1);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);
}

static void test_mkarticle_ok(void)
{
	char *root = new_root();
	char md[4096];
	char out[4096];
	char *av[9];
	char *got = NULL;
	struct opt o = opt_site("ca", "ca", NULL);
	int st;
	char *text;

	layout(root, POSTS_NONE, 1, json_ok);
	rel(root, "in.md", md, sizeof md);
	rel(root, "out.xml", out, sizeof out);
	put_text(md, md_ok);
	av[0] = mk_bin;
	av[1] = md;
	av[2] = "--source";
	av[3] = "blog/post/x.html";
	av[4] = "--lang";
	av[5] = "ca";
	av[6] = "-o";
	av[7] = out;
	av[8] = NULL;
	expect(root, av, o, 0);
	text = read_file(out, NULL);
	TEST_ASSERT_NOT_NULL(text);
	TEST_ASSERT_NOT_NULL(strstr(text, "data-sblg-source=\"blog/post/x.html\""));
	TEST_ASSERT_NOT_NULL(strstr(text, "<p>hi</p>"));
	TEST_ASSERT_NOT_NULL(strstr(text, "</article>"));
	free(text);

	av[1] = md;
	av[2] = "--lang";
	av[3] = "ca";
	av[4] = NULL;
	st = run_at(root, av, &o, &got);
	if (st != 0)
		dump_err(root);
	TEST_ASSERT_EQUAL_INT(0, st);
	TEST_ASSERT_NOT_NULL(got);
	TEST_ASSERT_NOT_NULL(strstr(got, "<p>hi</p>"));
	TEST_ASSERT_NOT_NULL(strstr(got, "</article>"));
	free(got);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);
}

static void test_mkarticle_errors(void)
{
	char *root = new_root();
	char md[4096];
	char *none[] = {mk_bin, NULL};
	char *badflag[] = {mk_bin, "--nope", NULL};
	char *missing[] = {mk_bin, "/no/such/portfolio.md", NULL};
	char *av[7];
	char path[4096];
	struct opt o = opt_site("ca", "ca", NULL);
	const char *baddate = "---\ndate: 2023-02-29\n---\nbody\n";

	layout(root, POSTS_NONE, 1, json_ok);
	expect(root, none, o, 1);
	expect(root, badflag, o, 1);
	expect(root, missing, o, 1);

	rel(root, "bad.md", md, sizeof md);
	put_text(md, baddate);
	av[0] = mk_bin;
	av[1] = md;
	av[2] = "--lang";
	av[3] = "en";
	av[4] = NULL;
	expect(root, av, o, 1);

	rel(root, "in.md", md, sizeof md);
	put_text(md, md_ok);
	rel(root, "bin/lowdown", path, sizeof path);
	put_exe(path, stub_fail);
	av[1] = md;
	av[3] = "ca";
	expect(root, av, o, 1);

	put_exe(path, lowdown_ok);
	av[2] = "--lang";
	av[3] = "ca";
	av[4] = "-o";
	av[5] = "/no/such/portfolio-mk-dir/out.xml";
	av[6] = NULL;
	expect(root, av, o, 1);

	put_exe(path, stub_fail);
	o.lowdown_mode = 1;
	av[2] = "--lang";
	av[3] = "ca";
	av[4] = NULL;
	expect(root, av, o, 1);
	o.lowdown_mode = 2;
	expect(root, av, o, 1);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(root));
	free(root);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_build_flow);
	RUN_TEST(test_build_variants);
	RUN_TEST(test_build_early_errors);
	RUN_TEST(test_build_input_errors);
	RUN_TEST(test_build_stub_errors);
	RUN_TEST(test_build_late_errors);
	RUN_TEST(test_mkarticle_ok);
	RUN_TEST(test_mkarticle_errors);
	return UNITY_END();
}
