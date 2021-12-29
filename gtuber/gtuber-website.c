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

/**
 * SECTION:gtuber-website
 * @title: GtuberWebsite
 * @short_description: a base class for creating plugins
 */

#include "gtuber-website.h"

#define parent_class gtuber_website_parent_class
G_DEFINE_TYPE (GtuberWebsite, gtuber_website, G_TYPE_OBJECT)
G_DEFINE_QUARK (gtuberwebsite-error-quark, gtuber_website_error)

static void gtuber_website_finalize (GObject *object);

static void gtuber_website_prepare (GtuberWebsite *website);
static GtuberFlow gtuber_website_create_request (GtuberWebsite *self,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error);
static GtuberFlow gtuber_website_read_response (GtuberWebsite *self,
    SoupMessage *msg, GError **error);
static GtuberFlow gtuber_website_parse_data (GtuberWebsite *self,
    gchar *data, GtuberMediaInfo *info, GError **error);
static GtuberFlow gtuber_website_parse_input_stream (GtuberWebsite *self,
    GInputStream *stream, GtuberMediaInfo *info, GError **error);
static GtuberFlow gtuber_website_set_user_req_headers (GtuberWebsite *self,
    SoupMessageHeaders *req_headers, GHashTable *user_headers, GError **error);

static void
gtuber_website_init (GtuberWebsite *self)
{
}

static void
gtuber_website_class_init (GtuberWebsiteClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtuberWebsiteClass *website_class = (GtuberWebsiteClass *) klass;

  gobject_class->finalize = gtuber_website_finalize;

  website_class->handles_input_stream = FALSE;
  website_class->prepare = gtuber_website_prepare;
  website_class->create_request = gtuber_website_create_request;
  website_class->read_response = gtuber_website_read_response;
  website_class->parse_data = gtuber_website_parse_data;
  website_class->parse_input_stream = gtuber_website_parse_input_stream;
  website_class->set_user_req_headers = gtuber_website_set_user_req_headers;
}

static void
gtuber_website_finalize (GObject *object)
{
  GtuberWebsite *self = GTUBER_WEBSITE (object);

  g_debug ("Website finalize");

  g_free (self->uri);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtuber_website_prepare (GtuberWebsite *website)
{
}

static GtuberFlow
gtuber_website_create_request (GtuberWebsite *self,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error)
{
  return (*error == NULL) ? GTUBER_FLOW_OK : GTUBER_FLOW_ERROR;
}

static GtuberFlow
gtuber_website_read_response (GtuberWebsite *self,
    SoupMessage *msg, GError **error)
{
  return (*error == NULL) ? GTUBER_FLOW_OK : GTUBER_FLOW_ERROR;
}

static GtuberFlow
gtuber_website_parse_data (GtuberWebsite *self,
    gchar *data, GtuberMediaInfo *info, GError **error)
{
  return (*error == NULL) ? GTUBER_FLOW_OK : GTUBER_FLOW_ERROR;
}

static GtuberFlow
gtuber_website_parse_input_stream (GtuberWebsite *self,
    GInputStream *stream, GtuberMediaInfo *info, GError **error)
{
  return (*error == NULL) ? GTUBER_FLOW_OK : GTUBER_FLOW_ERROR;
}

static void
insert_user_header (const gchar *name, const gchar *value, GHashTable *user_headers)
{
  gboolean addition;

  if (G_UNLIKELY (name == NULL)
      || !strcmp (name, "Accept-Encoding")
      || !strcmp (name, "Connection")
      || !strcmp (name, "Content-Length")
      || !strcmp (name, "Content-Type")
      || !strcmp (name, "Host"))
    return;

  addition = g_hash_table_insert (user_headers, g_strdup (name), g_strdup (value));
  g_debug ("%s user request header, %s: %s", addition ? "Inserted" : "Replaced", name, value);
}

static GtuberFlow
gtuber_website_set_user_req_headers (GtuberWebsite *self,
    SoupMessageHeaders *req_headers, GHashTable *user_headers, GError **error)
{
  if (*error)
    return GTUBER_FLOW_ERROR;

  soup_message_headers_foreach (req_headers,
      (SoupMessageHeadersForeachFunc) insert_user_header, user_headers);

  return GTUBER_FLOW_OK;
}

/**
 * gtuber_website_get_uri:
 * @website: a #GtuberWebsite
 *
 * Returns: (transfer none): current requested URI.
 */
const gchar *
gtuber_website_get_uri (GtuberWebsite *self)
{
  g_return_val_if_fail (GTUBER_IS_WEBSITE (self), NULL);

  return self->uri;
}

/**
 * gtuber_website_set_uri:
 * @website: a #GtuberWebsite
 * @uri: requested URI
 *
 * Set current requested URI.
 *
 * This is only useful for plugin implementations where
 * user requested URI needs to be altered, otherwise
 * #GtuberClient will set it automatically.
 */
void
gtuber_website_set_uri (GtuberWebsite *self, const gchar *uri)
{
  g_return_if_fail (GTUBER_IS_WEBSITE (self));

  g_free (self->uri);
  self->uri = g_strdup (uri);
}
