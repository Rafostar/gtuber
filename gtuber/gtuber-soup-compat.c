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

#include "gtuber-soup-compat.h"

#if !SOUP_CHECK_VERSION (2, 99, 1)

SoupStatus
soup_message_get_status (SoupMessage *msg)
{
  return msg->status_code;
}

SoupMessageHeaders *
soup_message_get_request_headers (SoupMessage *msg)
{
  return msg->request_headers;
}

SoupMessageHeaders *
soup_message_get_response_headers (SoupMessage *msg)
{
  return msg->response_headers;
}

void
soup_message_set_request_body (SoupMessage *msg, const char *content_type,
    GInputStream *stream, gssize content_length)
{
  GBytes *bytes;
  gchar *req_body;
  gsize req_size;

  bytes = g_input_stream_read_bytes (stream, content_length, NULL, NULL);
  req_body = g_bytes_unref_to_data (bytes, &req_size);

  soup_message_set_request (msg, content_type,
      SOUP_MEMORY_TAKE, req_body, req_size);
}

#endif /* SOUP_CHECK_VERSION */

GUri *
gtuber_soup_message_obtain_uri (SoupMessage *msg)
{
  GUri *guri;

#if !SOUP_CHECK_VERSION (2, 99, 1)
  SoupURI *uri;
  gchar *uri_str;

  uri = soup_message_get_uri (msg);
  uri_str = soup_uri_to_string (uri, FALSE);
  guri = g_uri_parse (uri_str, G_URI_FLAGS_ENCODED, NULL);

  g_free (uri_str);
#else
  guri = g_uri_ref (soup_message_get_uri (msg));
#endif

  return guri;
}
