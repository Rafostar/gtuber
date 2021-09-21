/*
 * Copyright (C) 2021 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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
_json_reader_va_iter (JsonReader *reader, const gchar *key, va_list args, guint *depth)
{
  const gchar *name;
  gboolean success = TRUE;

  name = key;
  while (success && name) {
    *depth += 1;
    if (!(success = json_reader_read_member (reader, name)))
      break;
    name = va_arg (args, const gchar *);
  }

  return success;
}

const gchar *
gtuber_utils_json_get_string (JsonReader *reader, const gchar *key, ...)
{
  va_list args;
  const gchar *value = NULL;
  guint depth = 0;
  gboolean success;

  va_start (args, key);
  success = _json_reader_va_iter (reader, key, args, &depth);
  va_end (args);

  if (success)
    value = json_reader_get_string_value (reader);

  while (depth--)
    json_reader_end_member (reader);

  return value;
}

gint64
gtuber_utils_json_get_int (JsonReader *reader, const gchar *key, ...)
{
  va_list args;
  gint64 value = 0;
  guint depth = 0;
  gboolean success;

  va_start (args, key);
  success = _json_reader_va_iter (reader, key, args, &depth);
  va_end (args);

  if (success)
    value = json_reader_get_int_value (reader);

  while (depth--)
    json_reader_end_member (reader);

  return value;
}

void
gtuber_utils_json_parser_debug (JsonParser *parser)
{
  gboolean would_drop = FALSE;

#if GLIB_CHECK_VERSION (2,68,0)
  would_drop = g_log_writer_default_would_drop (G_LOG_LEVEL_DEBUG, G_LOG_DOMAIN);
#endif

  if (!would_drop) {
    JsonGenerator *gen;
    gchar *data;

    gen = json_generator_new ();
    json_generator_set_pretty (gen, TRUE);
    json_generator_set_root (gen, json_parser_get_root (parser));
    data = json_generator_to_data (gen, NULL);

    g_debug ("Parser data: %s", data);

    g_free (data);
    g_object_unref (gen);
  }
}
