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

int main(int argc, char **argv)
{
	const char *src = NULL;
	const char *outpath = NULL;
	const char *source = NULL;
	const char *lang = NULL;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
			outpath = argv[++i];
		else if (strcmp(argv[i], "--source") == 0 && i + 1 < argc)
			source = argv[++i];
		else if (strcmp(argv[i], "--lang") == 0 && i + 1 < argc)
			lang = argv[++i];
		else if (argv[i][0] == '-')
			usage();
		else
			src = argv[i];
	}
	if (!src)
		usage();

	size_t len = 0;
	char *text = read_file(src, &len);
	if (!text)
		die("read %s: %s", src, strerror(errno));

	char *pre = NULL;
	size_t body = 0;
	if (article_prelude(text, len, source, lang, &pre, &body) != 0)
		die("article %s", src);

	char tmpl[] = "/tmp/mkarticleXXXXXX";
	int fd = mkstemp(tmpl);
	if (fd < 0)
		die("mkstemp: %s", strerror(errno));
	if (write_full(fd, text + body, len - body) != 0)
		die("write body: %s", strerror(errno));
	if (close(fd) != 0)
		die("close body: %s", strerror(errno));

	const char *ld = getenv("LOWDOWN");
	if (!ld || !ld[0])
		ld = "lowdown --html-no-skiphtml --html-no-escapehtml";

	char *html = NULL;
	size_t hlen = 0;
	int rc = run_cmd(ld, tmpl, &html, &hlen);
	unlink(tmpl);
	if (rc != 0)
		die("lowdown %s", src);

	size_t plen = strlen(pre);
	size_t total = plen + hlen + strlen("</article>\n");
	char *doc = malloc(total + 1);
	if (!doc)
		die("oom");
	memcpy(doc, pre, plen);
	memcpy(doc + plen, html, hlen);
	memcpy(doc + plen + hlen, "</article>\n", strlen("</article>\n") + 1);

	if (outpath) {
		if (write_file(outpath, doc, total) != 0)
			die("write %s: %s", outpath, strerror(errno));
	} else if (fwrite(doc, 1, total, stdout) != total)
		die("write stdout");

	free(text);
	free(pre);
	free(html);
	free(doc);
	return 0;
}
