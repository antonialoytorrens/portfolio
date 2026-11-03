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
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_flat);
	RUN_TEST(test_reject);
	return UNITY_END();
}
