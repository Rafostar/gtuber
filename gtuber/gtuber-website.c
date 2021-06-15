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

#include <glib-object.h>
#include <gmodule.h>

#include "gtuber-website.h"

#define parent_class gtuber_website_parent_class
G_DEFINE_TYPE (GtuberWebsite, gtuber_website, G_TYPE_OBJECT)
G_DEFINE_QUARK (gtuberwebsite-error-quark, gtuber_website_error)

static void gtuber_website_finalize (GObject *object);

static GtuberFlow gtuber_website_create_request (GtuberWebsite *self,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error);
static GtuberFlow gtuber_website_parse_response (GtuberWebsite *website,
    SoupMessage *msg, GtuberMediaInfo *info, GError **error);
static GtuberFlow gtuber_website_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error);

static void
gtuber_website_init (GtuberWebsite *self)
{
  self = gtuber_website_get_instance_private (self);

  self->uri = NULL;
  self->user_agent = NULL;
  self->browser_version = NULL;
}

static void
gtuber_website_class_init (GtuberWebsiteClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtuberWebsiteClass *website_class = (GtuberWebsiteClass *) klass;

  gobject_class->finalize = gtuber_website_finalize;

  website_class->handles_input_stream = FALSE;
  website_class->create_request = gtuber_website_create_request;
  website_class->parse_response = gtuber_website_parse_response;
  website_class->parse_input_stream = gtuber_website_parse_input_stream;
}

static void
gtuber_website_finalize (GObject *object)
{
  GtuberWebsite *self = GTUBER_WEBSITE (object);

  g_debug ("Website finalize");

  g_free (self->uri);
  g_free (self->user_agent);
  g_free (self->browser_version);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GtuberFlow
gtuber_website_create_request (GtuberWebsite *self,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error)
{
  return (*error == NULL) ? GTUBER_FLOW_OK : GTUBER_FLOW_ERROR;
}

static GtuberFlow
gtuber_website_parse_response (GtuberWebsite *website,
    SoupMessage *msg, GtuberMediaInfo *info, GError **error)
{
  return (*error == NULL) ? GTUBER_FLOW_OK : GTUBER_FLOW_ERROR;
}

static GtuberFlow
gtuber_website_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error)
{
  return (*error == NULL) ? GTUBER_FLOW_OK : GTUBER_FLOW_ERROR;
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

/**
 * gtuber_website_get_user_agent:
 * @website: a #GtuberWebsite
 *
 * Returns: (transfer none): advertised user agent string.
 */
const gchar *
gtuber_website_get_user_agent (GtuberWebsite *self)
{
  g_return_val_if_fail (GTUBER_IS_WEBSITE (self), NULL);

  return self->user_agent;
}

/**
 * gtuber_website_get_browser_version:
 * @website: a #GtuberWebsite
 *
 * Returns: (transfer none): advertised browser version.
 */
const gchar *
gtuber_website_get_browser_version (GtuberWebsite *self)
{
  g_return_val_if_fail (GTUBER_IS_WEBSITE (self), NULL);

  return self->browser_version;
}

/**
 * gtuber_website_set_browser:
 * @website: a #GtuberWebsite
 * @user_agent: an user agent string
 * @browser_version: web browser version
 *
 * Set current user agent and browser version strings.
 *
 * This is only useful for plugin implementations where
 * they need to be altered, otherwise #GtuberClient
 * will set them automatically.
 */
void
gtuber_website_set_browser (GtuberWebsite *self, const gchar *user_agent,
    const gchar *browser_version)
{
  g_return_if_fail (GTUBER_IS_WEBSITE (self));

  g_free (self->user_agent);
  self->user_agent = g_strdup (user_agent);

  g_free (self->browser_version);
  self->browser_version = g_strdup (browser_version);
}