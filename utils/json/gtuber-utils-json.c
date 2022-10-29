/*
 * Copyright (C) 2021 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "gtuber-utils-json.h"

static gboolean
_json_reader_va_iter (JsonReader *reader, va_list args, guint *depth)
{
  gboolean success = TRUE;

  while (success) {
    gpointer arg;

    if (!(arg = va_arg (args, gpointer)))
      break;

    if ((success = json_reader_is_object (reader))) {
      const gchar *name = (const gchar *) arg;

      *depth += 1;
      if (!(success = json_reader_read_member (reader, name)))
        break;
    } else if ((success = json_reader_is_array (reader))) {
      guint index = GPOINTER_TO_UINT (arg);

      /* Safety check */
      if (!(success = index > 0))
        break;

      index--;
      if (!(success = index < json_reader_count_elements (reader)))
        break;

      *depth += 1;
      if (!(success = json_reader_read_element (reader, index)))
        break;
    }
  }

  return success;
}

const gchar *
gtuber_utils_json_get_string (JsonReader *reader, ...)
{
  va_list args;
  const gchar *value = NULL;
  guint depth = 0;
  gboolean success;

  va_start (args, reader);
  success = _json_reader_va_iter (reader, args, &depth);
  va_end (args);

  /* Reading null as string makes reader stuck */
  if (success && json_reader_is_value (reader)
      && !json_reader_get_null_value (reader))
    value = json_reader_get_string_value (reader);

  gtuber_utils_json_go_back (reader, depth);

  return value;
}

gint64
gtuber_utils_json_get_int (JsonReader *reader, ...)
{
  va_list args;
  gint64 value = 0;
  guint depth = 0;
  gboolean success;

  va_start (args, reader);
  success = _json_reader_va_iter (reader, args, &depth);
  va_end (args);

  if (success && json_reader_is_value (reader))
    value = json_reader_get_int_value (reader);

  gtuber_utils_json_go_back (reader, depth);

  return value;
}

gboolean
gtuber_utils_json_get_boolean (JsonReader *reader, ...)
{
  va_list args;
  gboolean value = FALSE;
  guint depth = 0;
  gboolean success;

  va_start (args, reader);
  success = _json_reader_va_iter (reader, args, &depth);
  va_end (args);

  if (success && json_reader_is_value (reader))
    value = json_reader_get_boolean_value (reader);

  gtuber_utils_json_go_back (reader, depth);

  return value;
}

gint
gtuber_utils_json_count_elements (JsonReader *reader, ...)
{
  va_list args;
  gint n_elems = -1;
  guint depth = 0;
  gboolean success;

  va_start (args, reader);
  success = _json_reader_va_iter (reader, args, &depth);
  va_end (args);

  if (success && json_reader_is_array (reader))
    n_elems = json_reader_count_elements (reader);

  gtuber_utils_json_go_back (reader, depth);

  return n_elems;
}

gboolean
gtuber_utils_json_go_to (JsonReader *reader, ...)
{
  va_list args;
  guint depth = 0;
  gboolean success;

  va_start (args, reader);
  success = _json_reader_va_iter (reader, args, &depth);
  va_end (args);

  /* We do not go back here on success */
  if (!success)
    gtuber_utils_json_go_back (reader, depth);

  return success;
}

void
gtuber_utils_json_go_back (JsonReader *reader, guint count)
{
  /* FIXME: This is an abuse of json-glib mechanics, where
   * leaving array might position us in either parent array
   * or object thus result is the same as calling `end_member`
   * on an object. It would be nice if json-glib had an API
   * to check if we are currently inside of array */
  while (count--)
    json_reader_end_element (reader);
}

/**
 * gtuber_utils_json_array_foreach:
 * @reader: a #JsonReader
 * @info: (nullable): a #GtuberMediaInfo
 * @func: a #GtuberFunc to call for each array element
 * @user_data: user data to pass to the function
 *
 * Calls a function for each element of a array. Reader must
 * be at array each time callback function is called.
 *
 * Returns: %TRUE if array was found and iterated, %FALSE otherwise.
 */
gboolean
gtuber_utils_json_array_foreach (JsonReader *reader, GtuberMediaInfo *info, GtuberFunc func, gpointer user_data)
{
  guint i, count;

  if (!json_reader_is_array (reader))
    return FALSE;

  count = json_reader_count_elements (reader);
  for (i = 0; i < count; i++) {
    if (json_reader_read_element (reader, i)) {
      (*func) (reader, info, user_data);
    }
    json_reader_end_element (reader);
  }

  return (count > 0);
}

static gchar *
_json_parser_to_string_internal (JsonParser *parser, gboolean pretty)
{
  JsonGenerator *gen;
  gchar *data;

  gen = json_generator_new ();
  json_generator_set_pretty (gen, pretty);
  json_generator_set_root (gen, json_parser_get_root (parser));
  data = json_generator_to_data (gen, NULL);

  g_object_unref (gen);

  return data;
}

gchar *
gtuber_utils_json_parser_to_string (JsonParser *parser)
{
  return _json_parser_to_string_internal (parser, FALSE);
}

void
gtuber_utils_json_parser_debug (JsonParser *parser)
{
  gchar *data;

  if (g_log_writer_default_would_drop (G_LOG_LEVEL_DEBUG, G_LOG_DOMAIN))
    return;

  data = _json_parser_to_string_internal (parser, TRUE);

  g_debug ("Parser data: %s", data);
  g_free (data);
}
