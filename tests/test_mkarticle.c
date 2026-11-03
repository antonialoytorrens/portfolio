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

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_front);
	RUN_TEST(test_nobody);
	RUN_TEST(test_bad_date);
	return UNITY_END();
}
