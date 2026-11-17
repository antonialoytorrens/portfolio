#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void)
{
	die("usage: mkarticle <input.md> [-o out.xml] [--source path] [--lang ca|es|en]");
}

static int take_opt(const char *flag, int argc, char **argv, int *i, const char **dst)
{
	if (strcmp(argv[*i], flag) != 0 || *i + 1 >= argc)
		return 0;
	*dst = argv[++*i];
	return 1;
}

static void parse_args(int argc, char **argv, const char **src, const char **outpath,
	const char **source, const char **lang)
{
	int i;

	*src = NULL;
	*outpath = NULL;
	*source = NULL;
	*lang = NULL;
	for (i = 1; i < argc; i++) {
		if (take_opt("-o", argc, argv, &i, outpath))
			continue;
		if (take_opt("--source", argc, argv, &i, source))
			continue;
		if (take_opt("--lang", argc, argv, &i, lang))
			continue;
		if (argv[i][0] == '-')
			usage();
		*src = argv[i];
	}
	if (!*src)
		usage();
}

static char *join_html(const char *pre, const char *html, size_t hlen, size_t *total)
{
	size_t plen = strlen(pre);
	char *doc;

	*total = plen + hlen + strlen("</article>\n");
	doc = malloc(*total + 1);
	if (!doc)
		die("oom");
	memcpy(doc, pre, plen);
	memcpy(doc + plen, html, hlen);
	memcpy(doc + plen + hlen, "</article>\n", strlen("</article>\n") + 1);
	return doc;
}

static char *render_doc(const char *src, const char *source, const char *lang, size_t *total)
{
	size_t len = 0;
	char *text = read_file(src, &len);
	char *pre = NULL;
	size_t body = 0;
	char tmpl[] = "/tmp/mkarticleXXXXXX";
	int fd;
	const char *ld;
	char *html = NULL;
	size_t hlen = 0;
	int rc;
	char *doc;

	if (!text)
		die("read %s: %s", src, strerror(errno));
	if (article_prelude(text, len, source, lang, &pre, &body) != 0)
		die("article %s", src);
	fd = mkstemp(tmpl);
	if (fd < 0)
		die("mkstemp: %s", strerror(errno));
	if (write_full(fd, text + body, len - body) != 0)
		die("write body: %s", strerror(errno));
	if (close(fd) != 0)
		die("close body: %s", strerror(errno));
	ld = getenv("LOWDOWN");
	if (!ld || !ld[0])
		ld = "lowdown --html-no-skiphtml --html-no-escapehtml";
	rc = run_cmd(ld, tmpl, &html, &hlen);
	unlink(tmpl);
	if (rc != 0)
		die("lowdown %s", src);
	doc = join_html(pre, html, hlen, total);
	free(text);
	free(pre);
	free(html);
	return doc;
}

static void emit_doc(const char *outpath, const char *doc, size_t total)
{
	if (outpath) {
		if (write_file(outpath, doc, total) != 0)
			die("write %s: %s", outpath, strerror(errno));
		return;
	}
	if (fwrite(doc, 1, total, stdout) != total)
		die("write stdout");
}

int main(int argc, char **argv)
{
	const char *src;
	const char *outpath;
	const char *source;
	const char *lang;
	char *doc;
	size_t total = 0;

	parse_args(argc, argv, &src, &outpath, &source, &lang);
	doc = render_doc(src, source, lang, &total);
	emit_doc(outpath, doc, total);
	free(doc);
	return 0;
}
