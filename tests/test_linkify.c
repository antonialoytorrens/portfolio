#include "unity.h"
#include "util.h"

#include <stdlib.h>

void setUp(void) {}
void tearDown(void) {}

static void test_link(void)
{
	char *out = NULL;
	const char *in = "pre <span class=\"sblg-tag\">web</span> <span class=\"sblg-tag\">css</span>";
	const char *expect =
		"pre <a href=\"/ca/blog/tag/web.html\" class=\"sblg-tag\">web</a>"
		" <a href=\"/ca/blog/tag/css.html\" class=\"sblg-tag\">css</a>";

	TEST_ASSERT_EQUAL_INT(0, linkify(in, "ca", "blog", &out));
	TEST_ASSERT_EQUAL_STRING(expect, out);
	free(out);
}

static void test_tags(void)
{
	strvec *tags = NULL;

	TEST_ASSERT_EQUAL_INT(0, tags_collect("a\tweb\nb\tweb\nc\tcss\nlonely\n\n", &tags));
	TEST_ASSERT_EQUAL_UINT(3, strvec_len(tags));
	TEST_ASSERT_EQUAL_STRING("css", strvec_get(tags, 0));
	TEST_ASSERT_EQUAL_STRING("lonely", strvec_get(tags, 1));
	TEST_ASSERT_EQUAL_STRING("web", strvec_get(tags, 2));
	strvec_free(tags);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_link);
	RUN_TEST(test_tags);
	return UNITY_END();
}
