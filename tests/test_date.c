#include "unity.h"
#include "util.h"

void setUp(void) {}
void tearDown(void) {}

static void test_langs(void)
{
	char buf[64];

	TEST_ASSERT_EQUAL_INT(0, format_date_ymd("ca", "2024-01-05", buf, sizeof buf));
	TEST_ASSERT_EQUAL_STRING("5 gener de 2024", buf);
	TEST_ASSERT_EQUAL_INT(0, format_date_ymd("ca", "2024-03-08", buf, sizeof buf));
	TEST_ASSERT_EQUAL_STRING("8 març de 2024", buf);
	TEST_ASSERT_EQUAL_INT(0, format_date_ymd("es", "2024-01-05", buf, sizeof buf));
	TEST_ASSERT_EQUAL_STRING("5 de enero de 2024", buf);
	TEST_ASSERT_EQUAL_INT(0, format_date_ymd("en", "2024-12-15", buf, sizeof buf));
	TEST_ASSERT_EQUAL_STRING("December 15, 2024", buf);
	TEST_ASSERT_EQUAL_INT(0, format_date_ymd("en", "2024-01-05T00:00:00", buf, sizeof buf));
	TEST_ASSERT_EQUAL_STRING("January 5, 2024", buf);
}

static void test_reject(void)
{
	char buf[64];

	TEST_ASSERT_EQUAL_INT(-1, format_date_ymd("fr", "2024-01-05", buf, sizeof buf));
	TEST_ASSERT_EQUAL_INT(-1, format_date_ymd("en", "2024-13-01", buf, sizeof buf));
	TEST_ASSERT_EQUAL_INT(-1, format_date_ymd("en", "2023-02-29", buf, sizeof buf));
	TEST_ASSERT_EQUAL_INT(-1, format_date_ymd("en", "2024-02-30", buf, sizeof buf));
	TEST_ASSERT_EQUAL_INT(0, format_date_ymd("en", "2024-02-29", buf, sizeof buf));
	TEST_ASSERT_EQUAL_STRING("February 29, 2024", buf);
	TEST_ASSERT_EQUAL_INT(0, format_date_ymd("en", "2000-02-29", buf, sizeof buf));
	TEST_ASSERT_EQUAL_INT(-1, format_date_ymd("en", "1900-02-29", buf, sizeof buf));
}

static void test_year(void)
{
	TEST_ASSERT_EQUAL_INT(2023, year_from_time(1700000000));
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_langs);
	RUN_TEST(test_reject);
	RUN_TEST(test_year);
	return UNITY_END();
}
