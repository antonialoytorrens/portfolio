#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "util.h"

#include "stb_ds.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

struct strvec {
	char **v;
};

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

static int argv_ok(char *const *const argv)
{
	return argv && argv[0];
}

static void exec_or_exit(char *const argv[])
{
	PROFILE_FLUSH();
	execvp(argv[0], argv);
	fprintf(stderr, "exec %s: %s\n", argv[0], strerror(errno));
	PROFILE_FLUSH();
	_exit(127);
}

int spawn_wait(char *const argv[])
{
	pid_t pid;
	int st;

	if (!argv_ok(argv))
		return -1;
	pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0)
		exec_or_exit(argv);
	if (waitpid(pid, &st, 0) < 0)
		return -1;
	if (!WIFEXITED(st) || WEXITSTATUS(st) != 0)
		return -1;
	return 0;
}

static void capture_child(int pfd[2], char *const argv[])
{
	dup2(pfd[1], STDOUT_FILENO);
	close(pfd[0]);
	close(pfd[1]);
	exec_or_exit(argv);
}

static int capture_grow(char **buf, size_t n, size_t *cap)
{
	size_t c;
	char *grown;

	if (n + 4096 + 1 >= n && n + 4096 + 1 <= *cap)
		return 0;
	c = *cap ? *cap * 2 : 4096;
	if (n + 4096 + 1 >= n && c < n + 4096 + 1)
		c = n + 4096 + 1;
	grown = realloc(*buf, c);
	if (!grown)
		return -1;
	*buf = grown;
	*cap = c;
	return 0;
}

static char *capture_read(int fd, size_t *outn)
{
	char *buf = NULL;
	size_t n = 0;
	size_t cap = 0;

	for (;;) {
		ssize_t r;

		if (capture_grow(&buf, n, &cap) != 0) {
			free(buf);
			return NULL;
		}
		r = read(fd, buf + n, 4096);
		if (r < 0 && errno == EINTR)
			continue;
		if (r < 0) {
			free(buf);
			return NULL;
		}
		if (r == 0)
			break;
		n += (size_t)r;
	}
	*outn = n;
	return buf;
}

static int capture_finish(char *buf, size_t n, pid_t pid, char **out, size_t *outlen)
{
	int st;

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

int spawn_capture(char *const argv[], char **out, size_t *outlen)
{
	int pfd[2];
	pid_t pid;
	char *buf;
	size_t n = 0;
	int st;

	if (!argv_ok(argv))
		return -1;
	if (pipe(pfd) != 0)
		return -1;
	pid = fork();
	if (pid < 0) {
		close(pfd[0]);
		close(pfd[1]);
		return -1;
	}
	if (pid == 0)
		capture_child(pfd, argv);
	close(pfd[1]);
	buf = capture_read(pfd[0], &n);
	close(pfd[0]);
	if (!buf) {
		waitpid(pid, &st, 0);
		return -1;
	}
	return capture_finish(buf, n, pid, out, outlen);
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

static int argv_push(char ***av, size_t *n, size_t *cap, char *tok)
{
	if (*n + 3 > *cap) {
		size_t c = *cap ? *cap * 2 : 8;
		char **grown = realloc(*av, c * sizeof **av);

		if (!grown)
			return -1;
		*av = grown;
		*cap = c;
	}
	(*av)[(*n)++] = tok;
	return 0;
}

static int append_extra(char ***av, size_t *n, size_t cap, const char *extra)
{
	char **grown;

	if (!extra)
		return 0;
	if (*n + 2 > cap) {
		grown = realloc(*av, (*n + 2) * sizeof **av);
		if (!grown)
			return -1;
		*av = grown;
	}
	(*av)[(*n)++] = (char *)extra;
	return 0;
}

int run_cmd(const char *cmdline, const char *extra, char **out, size_t *outlen)
{
	/* strtok_r writes into the buffer; it cannot be const */
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
		if (argv_push(&av, &n, &cap, tok) != 0) {
			free(av);
			free(buf);
			return -1;
		}
	}
	if (append_extra(&av, &n, cap, extra) != 0) {
		free(av);
		free(buf);
		return -1;
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

static void tag_field(const char *p, size_t n, const char **field, size_t *flen)
{
	const char *tab;
	const char *tab2;

	if (n && p[n - 1] == '\r')
		n--;
	tab = memchr(p, '\t', n);
	if (!tab) {
		*field = p;
		*flen = n;
		return;
	}
	tab2 = memchr(tab + 1, '\t', (size_t)(p + n - (tab + 1)));
	*field = tab + 1;
	*flen = tab2 ? (size_t)(tab2 - *field) : (size_t)(p + n - *field);
}

static void push_field(strvec *raw, const char *field, size_t flen)
{
	char *s;

	if (!flen)
		return;
	s = malloc(flen + 1);
	if (!s)
		die("oom");
	memcpy(s, field, flen);
	s[flen] = 0;
	arrput(raw->v, s);
}

static void collect_lines(strvec *raw, const char *text)
{
	const char *p;

	for (p = text; *p;) {
		const char *nl = strchr(p, '\n');
		size_t n = nl ? (size_t)(nl - p) : strlen(p);
		const char *field;
		size_t flen;

		tag_field(p, n, &field, &flen);
		push_field(raw, field, flen);
		p = nl ? nl + 1 : p + n;
	}
}

static strvec *unique_sorted(strvec *raw)
{
	strvec *uniq = strvec_new();
	size_t i;

	strvec_sort(raw);
	for (i = 0; i < strvec_len(raw); i++) {
		if (i && strcmp(strvec_get(raw, i), strvec_get(raw, i - 1)) == 0)
			continue;
		strvec_push(uniq, strvec_get(raw, i));
	}
	return uniq;
}

int tags_collect(const char *text, strvec **out)
{
	strvec *raw = strvec_new();

	if (!text)
		text = "";
	collect_lines(raw, text);
	*out = unique_sorted(raw);
	strvec_free(raw);
	return 0;
}
