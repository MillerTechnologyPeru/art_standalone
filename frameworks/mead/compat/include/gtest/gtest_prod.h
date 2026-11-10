/*
 * gtest's gtest_prod.h, as far as Android 10's framework code uses it: the
 * macro that names a test class as a friend. No tests build here, so the
 * friend is never defined.
 */

#ifndef MEAD_GTEST_PROD_H
#define MEAD_GTEST_PROD_H

#define FRIEND_TEST(test_case_name, test_name) \
    friend class test_case_name##_##test_name##_Test

#endif
