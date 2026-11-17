#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "util.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int slurp_grow(char **buf, size_t n, size_t *cap)
{
	size_t c;
	char *grown;

	if (n + 4096 + 1 < n)
		return -1;
	if (n + 4096 + 1 <= *cap)
		return 0;
	c = *cap ? *cap * 2 : 4096;
	grown = realloc(*buf, c);
	if (!grown)
		return -1;
	*buf = grown;
	*cap = c;
	return 0;
}

static char *slurp(FILE *fp, size_t *n)
{
	char *buf = NULL;
	size_t cap = 0;

	*n = 0;
	for (;;) {
		size_t got;

		if (slurp_grow(&buf, *n, &cap) != 0) {
			free(buf);
			return NULL;
		}
		got = fread(buf + *n, 1, 4096, fp);
		*n += got;
		if (got < 4096) {
			if (ferror(fp)) {
				free(buf);
				return NULL;
			}
			break;
		}
	}
	return buf;
}

static char *slurp_finish(char *buf, size_t n, size_t *len)
{
	if (!buf)
		buf = malloc(1);
	if (!buf)
		return NULL;
	buf[n] = 0;
	if (len)
		*len = n;
	return buf;
}

char *read_file(const char *path, size_t *len)
{
	FILE *fp = fopen(path, "rb");
	char *buf;
	size_t n = 0;

	if (!fp)
		return NULL;
	buf = slurp(fp, &n);
	fclose(fp);
	if (!buf)
		return NULL;
	return slurp_finish(buf, n, len);
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

static int ensure_dir(const char *path)
{
	if (mkdir(path, 0755) != 0 && errno != EEXIST)
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
		if (ensure_dir(tmp) != 0) {
			free(tmp);
			return -1;
		}
		*p = '/';
	}
	if (ensure_dir(tmp) != 0) {
		free(tmp);
		return -1;
	}
	free(tmp);
	return 0;
}

static int copy_symlink(const char *src, const char *dst)
{
	char target[4096];
	ssize_t n = readlink(src, target, sizeof target - 1);

	if (n < 0)
		return -1;
	target[n] = 0;
	unlink(dst);
	if (symlink(target, dst) != 0)
		return -1;
	return 0;
}

static int copy_bytes(FILE *in, FILE *out)
{
	char blk[8192];
	size_t got;

	got = fread(blk, 1, sizeof blk, in);
	while (got > 0) {
		if (fwrite(blk, 1, got, out) != got)
			return -1;
		if (got < sizeof blk)
			break;
		got = fread(blk, 1, sizeof blk, in);
	}
	if (ferror(in))
		return -1;
	return 0;
}

static int copy_regular(const char *src, const char *dst, mode_t mode)
{
	FILE *in;
	FILE *out;

	in = fopen(src, "rb");
	if (!in)
		return -1;
	out = fopen(dst, "wb");
	if (!out) {
		fclose(in);
		return -1;
	}
	if (copy_bytes(in, out) != 0) {
		fclose(in);
		fclose(out);
		return -1;
	}
	fclose(in);
	if (fchmod(fileno(out), mode & 0777) != 0) {
		fclose(out);
		return -1;
	}
	if (fclose(out) != 0)
		return -1;
	return 0;
}

static int copy_one(const char *src, const char *dst)
{
	struct stat st;

	if (lstat(src, &st) != 0)
		return -1;
	if (S_ISLNK(st.st_mode))
		return copy_symlink(src, dst);
	return copy_regular(src, dst, st.st_mode);
}

static int copy_child(const char *src, const char *dst, const char *name)
{
	char from[4096];
	char to[4096];

	if (snprintf(from, sizeof from, "%s/%s", src, name) >= (int)sizeof from)
		return -1;
	if (snprintf(to, sizeof to, "%s/%s", dst, name) >= (int)sizeof to)
		return -1;
	if (copy_tree(from, to) != 0)
		return -1;
	return 0;
}

static int copy_dir(const char *src, const char *dst)
{
	DIR *d;
	const struct dirent *de;

	if (mkdir_p(dst) != 0)
		return -1;
	d = opendir(src);
	if (!d)
		return -1;
	while ((de = readdir(d)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;
		if (copy_child(src, dst, de->d_name) != 0) {
			closedir(d);
			return -1;
		}
	}
	closedir(d);
	return 0;
}

int copy_tree(const char *src, const char *dst)
{
	struct stat st;

	if (lstat(src, &st) != 0)
		return -1;
	if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode))
		return copy_one(src, dst);
	if (!S_ISDIR(st.st_mode))
		return -1;
	return copy_dir(src, dst);
}

static int rm_child(const char *path, const char *name)
{
	char child[4096];

	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return 0;
	if (snprintf(child, sizeof child, "%s/%s", path, name) >= (int)sizeof child)
		return -1;
	if (rm_rf(child) != 0)
		return -1;
	return 0;
}

static int rm_children(const char *path)
{
	DIR *d = opendir(path);
	const struct dirent *de;

	if (!d)
		return -1;
	while ((de = readdir(d)) != NULL) {
		if (rm_child(path, de->d_name) != 0) {
			closedir(d);
			return -1;
		}
	}
	closedir(d);
	if (rmdir(path) != 0)
		return -1;
	return 0;
}

int rm_rf(const char *path)
{
	struct stat st;

	if (lstat(path, &st) != 0)
		return errno == ENOENT ? 0 : -1;
	if (S_ISDIR(st.st_mode))
		return rm_children(path);
	if (unlink(path) != 0)
		return -1;
	return 0;
}
