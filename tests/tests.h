#pragma once

#include "gtuber/gtuber-plugin-devel.h"

#define GTUBER_TEST_MAIN()                                 \
int main (int argc, char **argv)                           \
{                                                          \
  GtuberClient *client = gtuber_client_new ();             \
  gint test_num = 0;                                       \
  if (argc > 1)                                            \
    test_num = g_ascii_strtoll (argv[argc - 1], NULL, 10); \
  run_test (client, test_num);                             \
  g_object_unref (client);                                 \
  return 0;                                                \
}

#define return_if_not_zero(a) \
if (a != 0) return;

#define assert_equals_int(a, b) g_assert_cmpint (a, ==, b)
#define assert_equals_string(a, b) g_assert_cmpstr (a, ==, b)

void compare_fetch (GtuberClient *client, const gchar *uri, GtuberMediaInfo *expect, GtuberMediaInfo **out);
