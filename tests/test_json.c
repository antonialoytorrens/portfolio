#include "unity.h"
#include "util.h"

void setUp(void) {}
void tearDown(void) {}

static void test_flat(void)
{
	kvmap *m = NULL;

	TEST_ASSERT_EQUAL_INT(0, json_load_mem("{\"a\":\"b\",\"c\":\"x\\\"y\\\\z\"}", &m));
	TEST_ASSERT_EQUAL_STRING("b", kv_get(m, "a"));
	TEST_ASSERT_EQUAL_STRING("x\"y\\z", kv_get(m, "c"));
	kv_free(m);
}

static void test_reject(void)
{
	kvmap *m = NULL;

	TEST_ASSERT_EQUAL_INT(-1, json_load_mem("{\"a\":1}", &m));
	TEST_ASSERT_NULL(m);
	TEST_ASSERT_EQUAL_INT(-1, json_load_mem("{\"a\":{\"b\":\"c\"}}", &m));
	TEST_ASSERT_NULL(m);
	TEST_ASSERT_EQUAL_INT(-1, json_load_mem("[\"a\"]", &m));
	TEST_ASSERT_NULL(m);
	TEST_ASSERT_EQUAL_INT(-1, json_load_mem(NULL, &m));
	TEST_ASSERT_EQUAL_INT(-1, json_load_mem("{", &m));
	TEST_ASSERT_EQUAL_INT(0, json_load_mem("{}", &m));
	TEST_ASSERT_NULL(kv_get(m, "a"));
	kv_free(m);
}

static void test_merge(void)
{
	kvmap *a = NULL;
	kvmap *b = NULL;
	kvmap *empty = NULL;

	TEST_ASSERT_EQUAL_INT(0, kv_new(&a));
	TEST_ASSERT_EQUAL_INT(0, kv_new(&b));
	TEST_ASSERT_EQUAL_INT(0, kv_new(&empty));
	TEST_ASSERT_EQUAL_INT(0, kv_put(a, "k", "1"));
	TEST_ASSERT_EQUAL_INT(0, kv_put(b, "k", "2"));
	TEST_ASSERT_EQUAL_INT(0, kv_put(b, "n", "3"));
	TEST_ASSERT_EQUAL_INT(0, kv_merge(a, NULL));
	TEST_ASSERT_EQUAL_INT(0, kv_merge(a, empty));
	TEST_ASSERT_EQUAL_INT(0, kv_merge(a, b));
	TEST_ASSERT_EQUAL_STRING("2", kv_get(a, "k"));
	TEST_ASSERT_EQUAL_STRING("3", kv_get(a, "n"));
	TEST_ASSERT_NULL(kv_get(a, "missing"));
	TEST_ASSERT_NULL(kv_get(NULL, "k"));
	kv_free(NULL);
	kv_free(a);
	kv_free(b);
	kv_free(empty);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_flat);
	RUN_TEST(test_reject);
	RUN_TEST(test_merge);
	return UNITY_END();
}
