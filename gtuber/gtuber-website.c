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
#include "gtuber-website-private.h"

struct _GtuberWebsitePrivate
{
  GUri *uri;
  gchar *uri_str;

  gchar *tmp_dir_path;

  SoupCookieJar *jar;
};

#define parent_class gtuber_website_parent_class
G_DEFINE_TYPE_WITH_CODE (GtuberWebsite, gtuber_website, G_TYPE_OBJECT,
    G_ADD_PRIVATE (GtuberWebsite))
G_DEFINE_QUARK (gtuberwebsite-error-quark, gtuber_website_error)

static void gtuber_website_dispose (GObject *object);
static void gtuber_website_finalize (GObject *object);

static void gtuber_website_prepare (GtuberWebsite *website);
static GtuberFlow gtuber_website_create_request (GtuberWebsite *self,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error);
static GtuberFlow gtuber_website_read_response (GtuberWebsite *self,
    SoupMessage *msg, GError **error);
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

  gobject_class->dispose = gtuber_website_dispose;
  gobject_class->finalize = gtuber_website_finalize;

  website_class->prepare = gtuber_website_prepare;
  website_class->create_request = gtuber_website_create_request;
  website_class->read_response = gtuber_website_read_response;
  website_class->parse_input_stream = gtuber_website_parse_input_stream;
  website_class->set_user_req_headers = gtuber_website_set_user_req_headers;
}

static gboolean
_rm_tmp_dir (GFile *dir)
{
  GFileEnumerator *dir_enum;
  gboolean success = FALSE;

  if (!(dir_enum = g_file_enumerate_children (dir,
      G_FILE_ATTRIBUTE_STANDARD_NAME,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL)))
    return success;

  while (TRUE) {
    GFile *child = NULL;

    if (!g_file_enumerator_iterate (dir_enum, NULL,
        &child, NULL, NULL) || !child)
      break;

    g_file_delete (child, NULL, NULL);
  }

  success = g_file_delete (dir, NULL, NULL);
  g_object_unref (dir_enum);

  return success;
}

static void
gtuber_website_dispose (GObject *object)
{
  GtuberWebsite *self = GTUBER_WEBSITE (object);
  GtuberWebsitePrivate *priv = gtuber_website_get_instance_private (self);

  /* Close DB connection before removing files */
  g_clear_object (&priv->jar);

  if (priv->tmp_dir_path) {
    GFile *tmp_dir;

    g_debug ("Removing temp dir: %s", priv->tmp_dir_path);

    tmp_dir = g_file_new_for_path (priv->tmp_dir_path);
    if (!_rm_tmp_dir (tmp_dir))
      g_debug ("Could not remove temp dir: %s", priv->tmp_dir_path);

    g_object_unref (tmp_dir);

    g_free (priv->tmp_dir_path);
    priv->tmp_dir_path = NULL;
  }
}

static void
gtuber_website_finalize (GObject *object)
{
  GtuberWebsite *self = GTUBER_WEBSITE (object);
  GtuberWebsitePrivate *priv = gtuber_website_get_instance_private (self);

  g_debug ("Website finalize");

  if (priv->uri)
    g_uri_unref (priv->uri);

  g_free (priv->uri_str);

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

void
gtuber_website_set_uri (GtuberWebsite *self, GUri *uri)
{
  GtuberWebsitePrivate *priv = gtuber_website_get_instance_private (self);

  if (priv->uri)
    g_uri_unref (priv->uri);

  priv->uri = g_uri_ref (uri);

  g_free (priv->uri_str);
  priv->uri_str = g_uri_to_string (priv->uri);
}

/**
 * gtuber_website_get_uri:
 * @website: a #GtuberWebsite
 *
 * Returns: (transfer none): current requested URI.
 */
GUri *
gtuber_website_get_uri (GtuberWebsite *self)
{
  GtuberWebsitePrivate *priv;

  g_return_val_if_fail (GTUBER_IS_WEBSITE (self), NULL);

  priv = gtuber_website_get_instance_private (self);

  return priv->uri;
}

/**
 * gtuber_website_get_uri_string:
 * @website: a #GtuberWebsite
 *
 * Returns: (transfer none): current requested URI as string.
 */
const gchar *
gtuber_website_get_uri_string (GtuberWebsite *self)
{
  GtuberWebsitePrivate *priv;

  g_return_val_if_fail (GTUBER_IS_WEBSITE (self), NULL);

  priv = gtuber_website_get_instance_private (self);

  return priv->uri_str;
}

/**
 * gtuber_website_get_use_http:
 * @website: a #GtuberWebsite
 *
 * Check if user provided URI indicates that unsecure HTTP
 * connection should be used.
 *
 * Returns: %TRUE if user requested HTTP, %FALSE otherwise.
 */
gboolean
gtuber_website_get_use_http (GtuberWebsite *self)
{
  GtuberWebsitePrivate *priv;

  g_return_val_if_fail (GTUBER_IS_WEBSITE (self), FALSE);

  priv = gtuber_website_get_instance_private (self);

  if (!priv->uri)
    return FALSE;

  return (g_uri_get_port (priv->uri) == 80
      || strcmp (g_uri_get_scheme (priv->uri), "http") == 0);
}

/**
 * gtuber_website_get_cookies_jar:
 * @website: a #GtuberWebsite
 *
 * Get #SoupCookieJar with user provided cookies.
 *
 * Note that first call into this function causes blocking I/O
 * as cookies are read. Next call will return the same jar,
 * so its safe to use this function multiple times without
 * getting a hold of the jar in the subclass.
 *
 * Returns: (nullable) (transfer none): A #SoupCookieJar with user
 *   provided cookies or %NULL when none.
 */
SoupCookieJar *
gtuber_website_get_cookies_jar (GtuberWebsite *self)
{
  GtuberWebsitePrivate *priv;

  g_return_val_if_fail (GTUBER_IS_WEBSITE (self), NULL);

  priv = gtuber_website_get_instance_private (self);

  if (!priv->jar) {
    GFile *cookies_file;
    gchar *cookies_path;

    cookies_path = gtuber_config_obtain_config_file_path ("cookies.sqlite");
    cookies_file = g_file_new_for_path (cookies_path);

    if (g_file_query_exists (cookies_file, NULL)) {
      GError *error = NULL;

      g_debug ("Creating cookies jar");

      if (!priv->tmp_dir_path)
        priv->tmp_dir_path = g_dir_make_tmp ("gtuber_XXXXXX", &error);

      if (!error) {
        GFile *tmp_file;
        gchar *tmp_filename;

        tmp_filename = g_build_filename (priv->tmp_dir_path, "cookies.sqlite", NULL);
        tmp_file = g_file_new_for_path (tmp_filename);

        if (g_file_copy (cookies_file, tmp_file, G_FILE_COPY_NONE, NULL, NULL, NULL, &error)) {
          if ((priv->jar = soup_cookie_jar_db_new (tmp_filename, FALSE)))
            g_debug ("Created cookies jar with DB file: %s", tmp_filename);
        } else {
          g_warning ("Could not copy cookies file into tmp, reason: %s", error->message);
        }

        g_object_unref (tmp_file);
        g_free (tmp_filename);
      } else {
        g_warning ("Could not prepare cookies tmp file, reason: %s", error->message);
      }

      g_clear_error (&error);
    }

    g_object_unref (cookies_file);
    g_free (cookies_path);
  }

  return priv->jar;
}
