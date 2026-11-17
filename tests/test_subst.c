#include "unity.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

void setUp(void) {}
void tearDown(void) {}

static void test_subst(void)
{
	kvmap *m = NULL;
	char *out = NULL;

	TEST_ASSERT_EQUAL_INT(0, kv_new(&m));
	TEST_ASSERT_EQUAL_INT(0, kv_put(m, "title.home", "Inici"));
	TEST_ASSERT_EQUAL_INT(0, kv_put(m, "page_title", "JSON"));
	TEST_ASSERT_EQUAL_INT(0, kv_put(m, "page_title", "Foo"));
	TEST_ASSERT_EQUAL_INT(0, kv_put(m, "x", "@@y@@"));
	TEST_ASSERT_EQUAL_INT(0, kv_put(m, "y", "Z"));
	TEST_ASSERT_EQUAL_INT(0, subst_apply("@@title.home@@ @@page_title@@ @@missing@@ @@x@@", m, &out));
	TEST_ASSERT_EQUAL_STRING("Inici Foo @@missing@@ @@y@@", out);
	free(out);
	kv_free(m);
}

static void test_inject(void)
{
	inject_item item;
	char *out = NULL;
	FILE *fp;

	fp = fopen("bin/fixture-inject.txt", "wb");
	TEST_ASSERT_NOT_NULL(fp);
	fputs("ITEM\n", fp);
	fclose(fp);
	item.key = "post_item";
	item.path = "bin/fixture-inject.txt";
	TEST_ASSERT_EQUAL_INT(0, inject_apply("  @@post_item@@\nnext\n", &item, 1, &out));
	TEST_ASSERT_EQUAL_STRING("ITEM\nnext\n", out);
	free(out);
	TEST_ASSERT_EQUAL_INT(-1, inject_apply(NULL, &item, 1, &out));
	TEST_ASSERT_EQUAL_INT(0, inject_apply("plain", &item, 0, &out));
	TEST_ASSERT_EQUAL_STRING("plain", out);
	free(out);
	item.key = "missing";
	item.path = "/no/such/portfolio-inject";
	TEST_ASSERT_EQUAL_INT(-1, inject_apply("@@missing@@\n", &item, 1, &out));
}

static void test_subst_edges(void)
{
	kvmap *m = NULL;
	char *out = NULL;

	TEST_ASSERT_EQUAL_INT(0, kv_new(&m));
	TEST_ASSERT_EQUAL_INT(-1, subst_apply(NULL, m, &out));
	TEST_ASSERT_EQUAL_INT(0, subst_apply("plain", m, &out));
	TEST_ASSERT_EQUAL_STRING("plain", out);
	free(out);
	TEST_ASSERT_EQUAL_INT(0, subst_apply("@@open", m, &out));
	TEST_ASSERT_EQUAL_STRING("@@open", out);
	free(out);
	kv_free(m);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_subst);
	RUN_TEST(test_subst_edges);
	RUN_TEST(test_inject);
	return UNITY_END();
}
