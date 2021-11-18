#pragma once

#include <gtuber/gtuber-plugin-devel.h>

#define GTUBER_TEST_MAIN_START()                           \
int main (int argc, char **argv)                           \
{                                                          \
  GtuberClient *client = gtuber_client_new ();             \
  gint _arg_num = 0;                                       \
  if (argc > 1)                                            \
    _arg_num = g_ascii_strtoll (argv[argc - 1], NULL, 10);

#define GTUBER_TEST_CASE(_num)                             \
  if (_num == _arg_num || _arg_num == 0)

#define GTUBER_TEST_MAIN_END()                             \
  g_object_unref (client);                                 \
  return 0;                                                \
}

#define assert_equals_int(a, b) g_assert_cmpint (a, ==, b)
#define assert_equals_string(a, b) g_assert_cmpstr (a, ==, b)

void compare_fetch (GtuberClient *client, const gchar *uri, GtuberMediaInfo *expect, GtuberMediaInfo **out);

void check_streams (GtuberMediaInfo *info);

void check_adaptive_streams (GtuberMediaInfo *info);

void assert_no_streams (GtuberMediaInfo *info);

void assert_no_adaptive_streams (GtuberMediaInfo *info);
