#include "unity.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_front(void)
{
	const char *text =
		"---\n"
		"title: A & B\n"
		"date: 2024-01-05T12:00:00\n"
		"tags: web css\n"
		"description: Fish <cod>\n"
		"img: /a.png\n"
		"author: Me\n"
		"---\n"
		"body\n";
	char *pre = NULL;
	size_t off = 0;
	const char *expect =
		"<article data-sblg-article=\"1\" data-sblg-source=\"blog/post/x.html\""
		" data-sblg-title=\"A &amp; B\" data-sblg-datetime=\"2024-01-05\""
		" data-sblg-tags=\"web css\" data-sblg-img=\"/a.png\""
		" data-sblg-set-datefmt=\"5 gener de 2024\" data-sblg-set-author=\"Me\">\n"
		"  <aside>Fish &lt;cod&gt;</aside>\n";

	TEST_ASSERT_EQUAL_INT(0, article_prelude(text, strlen(text), "blog/post/x.html", "ca", &pre, &off));
	TEST_ASSERT_EQUAL_STRING(expect, pre);
	TEST_ASSERT_EQUAL_STRING("body\n", text + off);
	free(pre);
}

static void test_nobody(void)
{
	const char *text = "hello\n";
	char *pre = NULL;
	size_t off = 99;

	TEST_ASSERT_EQUAL_INT(0, article_prelude(text, strlen(text), NULL, NULL, &pre, &off));
	TEST_ASSERT_EQUAL_STRING("<article data-sblg-article=\"1\">\n", pre);
	TEST_ASSERT_EQUAL_UINT(0, off);
	free(pre);
}

static void test_bad_date(void)
{
	const char *text = "---\ndate: 2023-02-29\n---\n";
	char *pre = NULL;
	size_t off = 0;

	TEST_ASSERT_EQUAL_INT(-1, article_prelude(text, strlen(text), NULL, "en", &pre, &off));
	TEST_ASSERT_EQUAL_INT(-1, article_prelude(text, strlen(text), NULL, "fr", &pre, &off));
}

static void test_more(void)
{
	const char *quoted = "--- \ntitle: Say \"hi\" >\n---\n";
	const char *empty = "---\ndate:\n---\n";
	char *pre = NULL;
	size_t off = 0;

	TEST_ASSERT_EQUAL_INT(0, article_prelude("x", 1, NULL, NULL, &pre, &off));
	TEST_ASSERT_EQUAL_UINT(0, off);
	free(pre);
	TEST_ASSERT_EQUAL_INT(0, article_prelude(quoted, strlen(quoted), "", "en", &pre, &off));
	TEST_ASSERT_NOT_NULL(strstr(pre, "Say &quot;hi&quot; &gt;"));
	TEST_ASSERT_NULL(strstr(pre, "data-sblg-source"));
	free(pre);
	TEST_ASSERT_EQUAL_INT(0, article_prelude(empty, strlen(empty), NULL, "en", &pre, &off));
	TEST_ASSERT_NOT_NULL(strstr(pre, "data-sblg-datetime=\"\""));
	free(pre);

	{
		const char *spaced = "---\t\r\ntitle:  hi \t\n---\n";
		const char *nodate = "---\ndate: 2024-01-05\n---\n";

		TEST_ASSERT_EQUAL_INT(0, article_prelude(spaced, strlen(spaced), NULL, "en", &pre, &off));
		TEST_ASSERT_NOT_NULL(strstr(pre, "data-sblg-title=\"hi\""));
		free(pre);
		TEST_ASSERT_EQUAL_INT(-1, article_prelude(nodate, strlen(nodate), NULL, "", &pre, &off));
	}
	{
		const char *nocolon = "---\nnotakey\ntitle: T\n---\n";
		const char *eofence = "---\ntitle: End\n---";
		const char *emptyk = "---\n: skip\ntitle: K\n---\n";
		const char *badfence = "---x\ntitle: T\n---\n";

		TEST_ASSERT_EQUAL_INT(0, article_prelude(nocolon, strlen(nocolon), NULL, "en", &pre, &off));
		TEST_ASSERT_NOT_NULL(strstr(pre, "data-sblg-title=\"T\""));
		free(pre);
		TEST_ASSERT_EQUAL_INT(0, article_prelude(eofence, strlen(eofence), NULL, "en", &pre, &off));
		TEST_ASSERT_EQUAL_UINT(strlen(eofence), off);
		TEST_ASSERT_NOT_NULL(strstr(pre, "data-sblg-title=\"End\""));
		free(pre);
		TEST_ASSERT_EQUAL_INT(0, article_prelude(emptyk, strlen(emptyk), NULL, "en", &pre, &off));
		TEST_ASSERT_NOT_NULL(strstr(pre, "data-sblg-title=\"K\""));
		TEST_ASSERT_NULL(strstr(pre, "skip"));
		free(pre);
		TEST_ASSERT_EQUAL_INT(0, article_prelude(badfence, strlen(badfence), NULL, NULL, &pre, &off));
		TEST_ASSERT_NULL(strstr(pre, "data-sblg-title"));
		free(pre);
		TEST_ASSERT_EQUAL_INT(0, article_prelude("---", 3, NULL, NULL, &pre, &off));
		TEST_ASSERT_EQUAL_UINT(3, off);
		free(pre);
	}
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_front);
	RUN_TEST(test_nobody);
	RUN_TEST(test_bad_date);
	RUN_TEST(test_more);
	return UNITY_END();
}
