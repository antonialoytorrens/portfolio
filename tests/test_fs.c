#include "unity.h"
#include "util.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void setUp(void) {}
void tearDown(void) {}

static char *tmpdir(void)
{
	char tmpl[] = "/tmp/portfolio-fsXXXXXX";
	char *made = mkdtemp(tmpl);

	TEST_ASSERT_NOT_NULL(made);
	return strdup(made);
}

static void join(char *dst, size_t n, const char *dir, const char *name)
{
	snprintf(dst, n, "%s/%s", dir, name);
}

static void test_files(void)
{
	char *dir = tmpdir();
	char path[512];
	char big[9000];
	char copy[512];
	char *got;
	size_t n = 0;
	int fd;

	join(path, sizeof path, dir, "a.txt");
	TEST_ASSERT_EQUAL_INT(0, write_file(path, "hi", 2));
	got = read_file(path, &n);
	TEST_ASSERT_NOT_NULL(got);
	TEST_ASSERT_EQUAL_UINT(2, n);
	TEST_ASSERT_EQUAL_STRING("hi", got);
	free(got);

	memset(big, 'x', sizeof big);
	TEST_ASSERT_EQUAL_INT(0, write_file(path, big, sizeof big));
	got = read_file(path, &n);
	TEST_ASSERT_NOT_NULL(got);
	TEST_ASSERT_EQUAL_UINT(sizeof big, n);
	TEST_ASSERT_EQUAL_INT(0, memcmp(got, big, sizeof big));
	free(got);
	join(copy, sizeof copy, dir, "copy.bin");
	TEST_ASSERT_EQUAL_INT(0, copy_tree(path, copy));
	got = read_file(copy, &n);
	TEST_ASSERT_EQUAL_UINT(sizeof big, n);
	free(got);

	join(path, sizeof path, dir, "empty");
	TEST_ASSERT_EQUAL_INT(0, write_file(path, "", 0));
	got = read_file(path, &n);
	TEST_ASSERT_NOT_NULL(got);
	TEST_ASSERT_EQUAL_UINT(0, n);
	TEST_ASSERT_EQUAL_STRING("", got);
	free(got);
	got = read_file(path, NULL);
	TEST_ASSERT_NOT_NULL(got);
	free(got);
	TEST_ASSERT_NULL(read_file("/no/such/portfolio-file", NULL));
	TEST_ASSERT_EQUAL_INT(-1, write_file("/no/such/portfolio-dir/x", "a", 1));
	TEST_ASSERT_EQUAL_INT(-1, write_full(-1, "a", 1));
	fd = open(path, O_WRONLY | O_TRUNC);
	TEST_ASSERT_TRUE(fd >= 0);
	TEST_ASSERT_EQUAL_INT(0, write_full(fd, "abcd", 4));
	TEST_ASSERT_EQUAL_INT(0, close(fd));
	got = read_file(path, NULL);
	TEST_ASSERT_EQUAL_STRING("abcd", got);
	free(got);
	TEST_ASSERT_EQUAL_INT(0, rm_rf(dir));
	free(dir);
}

static void test_tree(void)
{
	char *dir = tmpdir();
	char src[512];
	char dst[512];
	char file[512];
	char sub[512];
	char link[512];
	char copied[512];
	char target[64];
	char *got;
	ssize_t n;

	join(src, sizeof src, dir, "src");
	TEST_ASSERT_EQUAL_INT(0, mkdir_p(src));
	TEST_ASSERT_EQUAL_INT(0, mkdir_p(src));
	join(sub, sizeof sub, src, "sub");
	TEST_ASSERT_EQUAL_INT(0, mkdir_p(sub));
	join(file, sizeof file, src, "f");
	TEST_ASSERT_EQUAL_INT(0, write_file(file, "z", 1));
	join(link, sizeof link, src, "l");
	TEST_ASSERT_EQUAL_INT(0, symlink("f", link));
	join(dst, sizeof dst, dir, "dst");
	TEST_ASSERT_EQUAL_INT(0, copy_tree(src, dst));
	join(copied, sizeof copied, dst, "f");
	got = read_file(copied, NULL);
	TEST_ASSERT_EQUAL_STRING("z", got);
	free(got);
	join(copied, sizeof copied, dst, "sub/g");
	join(file, sizeof file, sub, "g");
	TEST_ASSERT_EQUAL_INT(0, write_file(file, "g", 1));
	TEST_ASSERT_EQUAL_INT(0, rm_rf(dst));
	TEST_ASSERT_EQUAL_INT(0, copy_tree(src, dst));
	got = read_file(copied, NULL);
	TEST_ASSERT_EQUAL_STRING("g", got);
	free(got);
	join(copied, sizeof copied, dst, "l");
	n = readlink(copied, target, sizeof target - 1);
	TEST_ASSERT_TRUE(n > 0);
	target[n] = 0;
	TEST_ASSERT_EQUAL_STRING("f", target);
	join(file, sizeof file, dir, "one");
	TEST_ASSERT_EQUAL_INT(0, write_file(file, "0123456789abcdef", 16));
	join(copied, sizeof copied, dir, "two");
	TEST_ASSERT_EQUAL_INT(0, copy_tree(file, copied));
	got = read_file(copied, NULL);
	TEST_ASSERT_EQUAL_STRING("0123456789abcdef", got);
	free(got);
	TEST_ASSERT_EQUAL_INT(-1, copy_tree("/no/such/portfolio-tree", dst));
	join(file, sizeof file, dir, "blocked");
	TEST_ASSERT_EQUAL_INT(0, write_file(file, "x", 1));
	snprintf(src, sizeof src, "%s/blocked/child", dir);
	TEST_ASSERT_EQUAL_INT(-1, mkdir_p(src));
	TEST_ASSERT_EQUAL_INT(0, rm_rf(file));
	TEST_ASSERT_EQUAL_INT(0, rm_rf(file));
	TEST_ASSERT_EQUAL_INT(0, rm_rf(dir));
	free(dir);
}

static void test_errors(void)
{
	char *dir = tmpdir();
	char file[512];
	char src[512];
	char inner[512];
	char dst[512];
	char nested[512];
	char link[512];
	char big[65536];
	int rc;

	memset(big, 'a', sizeof big);
	join(file, sizeof file, dir, "f");
	TEST_ASSERT_EQUAL_INT(0, write_file(file, "abc", 3));
	TEST_ASSERT_EQUAL_INT(-1, write_file("/dev/full", big, sizeof big));
	TEST_ASSERT_EQUAL_INT(0, write_file(file, big, sizeof big));
	TEST_ASSERT_EQUAL_INT(-1, copy_tree(file, "/dev/full"));
	TEST_ASSERT_EQUAL_INT(0, chmod(file, 0));
	join(dst, sizeof dst, dir, "out");
	rc = copy_tree(file, dst);
	TEST_ASSERT_EQUAL_INT(0, chmod(file, 0644));
	TEST_ASSERT_EQUAL_INT(-1, rc);

	join(src, sizeof src, dir, "src");
	TEST_ASSERT_EQUAL_INT(0, mkdir_p(src));
	join(inner, sizeof inner, src, "secret");
	TEST_ASSERT_EQUAL_INT(0, write_file(inner, "s", 1));
	TEST_ASSERT_EQUAL_INT(0, chmod(inner, 0));
	rc = copy_tree(src, dst);
	TEST_ASSERT_EQUAL_INT(0, chmod(inner, 0644));
	TEST_ASSERT_EQUAL_INT(-1, rc);

	join(link, sizeof link, dir, "l");
	TEST_ASSERT_EQUAL_INT(0, symlink("f", link));
	TEST_ASSERT_EQUAL_INT(-1, copy_tree(link, "/no/such/portfolio-parent/x"));

	TEST_ASSERT_EQUAL_INT(0, chmod(dir, 0555));
	snprintf(nested, sizeof nested, "%s/nope/child", dir);
	rc = mkdir_p(nested);
	TEST_ASSERT_EQUAL_INT(0, chmod(dir, 0755));
	TEST_ASSERT_EQUAL_INT(-1, rc);

	TEST_ASSERT_EQUAL_INT(0, rm_rf(dir));
	free(dir);
}

static void test_special(void)
{
	char *dir = tmpdir();
	char src[512];
	char dst[512];
	char fifo[512];
	char link[512];
	char file[512];
	char target[64];
	char *big;
	int fds[2];
	size_t n = 256 * 1024;
	ssize_t got;

	join(fifo, sizeof fifo, dir, "fifo");
	TEST_ASSERT_EQUAL_INT(0, mkfifo(fifo, 0644));
	join(dst, sizeof dst, dir, "fifo-copy");
	TEST_ASSERT_EQUAL_INT(-1, copy_tree(fifo, dst));

	join(src, sizeof src, dir, "locked");
	TEST_ASSERT_EQUAL_INT(0, mkdir_p(src));
	join(file, sizeof file, src, "f");
	TEST_ASSERT_EQUAL_INT(0, write_file(file, "z", 1));
	TEST_ASSERT_EQUAL_INT(0, chmod(src, 0));
	join(dst, sizeof dst, dir, "locked-copy");
	TEST_ASSERT_EQUAL_INT(-1, copy_tree(src, dst));
	TEST_ASSERT_EQUAL_INT(-1, rm_rf(src));
	TEST_ASSERT_EQUAL_INT(0, chmod(src, 0755));

	TEST_ASSERT_NULL(read_file(dir, NULL));

	join(file, sizeof file, dir, "old");
	TEST_ASSERT_EQUAL_INT(0, write_file(file, "old", 3));
	join(link, sizeof link, dir, "lnk");
	TEST_ASSERT_EQUAL_INT(0, symlink("target", link));
	TEST_ASSERT_EQUAL_INT(0, copy_tree(link, file));
	got = readlink(file, target, sizeof target - 1);
	TEST_ASSERT_TRUE(got > 0);
	target[got] = 0;
	TEST_ASSERT_EQUAL_STRING("target", target);

	{
		char bigfile[512];
		char block[20000];

		memset(block, 'z', sizeof block);
		join(bigfile, sizeof bigfile, dir, "big");
		TEST_ASSERT_EQUAL_INT(0, write_file(bigfile, block, sizeof block));
		{
			size_t gotn = 0;

			big = read_file(bigfile, &gotn);
			TEST_ASSERT_NOT_NULL(big);
			TEST_ASSERT_EQUAL_UINT(sizeof block, gotn);
			free(big);
		}
	}

	join(file, sizeof file, dir, "reg");
	TEST_ASSERT_EQUAL_INT(0, write_file(file, "z", 1));
	TEST_ASSERT_EQUAL_INT(-1, copy_tree(file, "/no/such/portfolio-parent/x"));
	TEST_ASSERT_EQUAL_INT(-1, copy_tree(src, "/no/such/portfolio-parent/dir"));

	TEST_ASSERT_EQUAL_INT(0, pipe(fds));
	TEST_ASSERT_EQUAL_INT(0, fcntl(fds[1], F_SETFL, O_NONBLOCK));
	big = malloc(n);
	TEST_ASSERT_NOT_NULL(big);
	memset(big, 'q', n);
	TEST_ASSERT_EQUAL_INT(-1, write_full(fds[1], big, n));
	free(big);
	TEST_ASSERT_EQUAL_INT(0, close(fds[0]));
	TEST_ASSERT_EQUAL_INT(0, close(fds[1]));

	TEST_ASSERT_EQUAL_INT(0, rm_rf(dir));
	free(dir);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_files);
	RUN_TEST(test_tree);
	RUN_TEST(test_errors);
	RUN_TEST(test_special);
	return UNITY_END();
}
