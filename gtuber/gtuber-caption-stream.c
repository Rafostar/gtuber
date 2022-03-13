/*
 * Copyright (C) 2022 Rafał Dzięgiel <rafostar.github@gmail.com>
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
 * SECTION:gtuber-caption-stream
 * @title: GtuberCaptionStream
 * @short_description: represents a caption stream
 */

/**
 * SECTION:gtuber-caption-stream-devel
 * @title: GtuberCaptionStream Development
 */

#include "gtuber-stream.h"
#include "gtuber-stream-private.h"
#include "gtuber-caption-stream.h"
#include "gtuber-caption-stream-devel.h"
#include "gtuber-caption-stream-private.h"

enum
{
  PROP_0,
  PROP_LANG_CODE,
  PROP_LAST
};

#define parent_class gtuber_caption_stream_parent_class
G_DEFINE_TYPE (GtuberCaptionStream, gtuber_caption_stream, GTUBER_TYPE_STREAM);

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void gtuber_caption_stream_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec);
static void gtuber_caption_stream_finalize (GObject *object);

static void
gtuber_caption_stream_init (GtuberCaptionStream *self)
{
}

static void
gtuber_caption_stream_class_init (GtuberCaptionStreamClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->get_property = gtuber_caption_stream_get_property;
  gobject_class->finalize = gtuber_caption_stream_finalize;

  param_specs[PROP_LANG_CODE] = g_param_spec_string ("lang-code",
      "Lang Code", "Caption language code", NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}

static void
gtuber_caption_stream_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  GtuberCaptionStream *self = GTUBER_CAPTION_STREAM (object);

  switch (prop_id) {
    case PROP_LANG_CODE:
      g_value_set_string (value, gtuber_caption_stream_get_lang_code (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gtuber_caption_stream_finalize (GObject *object)
{
  GtuberCaptionStream *self = GTUBER_CAPTION_STREAM (object);

  g_free (self->lang_code);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gtuber_caption_stream_new:
 *
 * Creates a new #GtuberCaptionStream instance.
 *
 * This is mainly useful for plugin development.
 *
 * Returns: (transfer full): a new #GtuberCaptionStream instance.
 */
GtuberCaptionStream *
gtuber_caption_stream_new (void)
{
  return g_object_new (GTUBER_TYPE_CAPTION_STREAM, NULL);
}

/**
 * gtuber_caption_stream_get_lang_code:
 * @stream: a #GtuberCaptionStream
 *
 * Returns: (transfer none): Language code of the stream.
 */
const gchar *
gtuber_caption_stream_get_lang_code (GtuberCaptionStream *self)
{
  g_return_val_if_fail (GTUBER_IS_CAPTION_STREAM (self), NULL);

  return self->lang_code;
}

/**
 * gtuber_caption_stream_set_lang_code:
 * @stream: a #GtuberCaptionStream
 * @lang_code: a lang code
 *
 * Sets the caption stream language code.
 *
 * This is mainly useful for plugin development.
 */
void
gtuber_caption_stream_set_lang_code (GtuberCaptionStream *self, const gchar *lang_code)
{
  g_return_if_fail (GTUBER_IS_CAPTION_STREAM (self));

  g_free (self->lang_code);
  self->lang_code = g_strdup (lang_code);
}
