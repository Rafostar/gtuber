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

#include "gtuber-media-info.h"
#include "gtuber-stream-private.h"
#include "gtuber-adaptive-stream-private.h"

enum
{
  PROP_0,
  PROP_ID,
  PROP_TITLE,
  PROP_DESCRIPTION,
  PROP_DURATION,
  PROP_LAST
};

struct _GtuberMediaInfo
{
  GObject parent;

  gchar *id;
  gchar *title;
  gchar *description;
  guint64 duration;

  GPtrArray *streams;
  GPtrArray *adaptive_streams;
};

struct _GtuberMediaInfoClass
{
  GObjectClass parent_class;
};

#define parent_class gtuber_media_info_parent_class
G_DEFINE_TYPE (GtuberMediaInfo, gtuber_media_info, G_TYPE_OBJECT);

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void gtuber_media_info_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec);
static void gtuber_media_info_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec);
static void gtuber_media_info_finalize (GObject *object);

static void
gtuber_media_info_init (GtuberMediaInfo *self)
{
  self = gtuber_media_info_get_instance_private (self);

  self->id = NULL;
  self->title = NULL;
  self->duration = 0;

  self->streams =
      g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
  self->adaptive_streams =
      g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

static void
gtuber_media_info_class_init (GtuberMediaInfoClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->get_property = gtuber_media_info_get_property;
  gobject_class->set_property = gtuber_media_info_set_property;
  gobject_class->finalize = gtuber_media_info_finalize;

  param_specs[PROP_ID] = g_param_spec_string ("id",
      "ID", "The ID of media", NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_TITLE] = g_param_spec_string ("title",
      "Title", "Media title", NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_DESCRIPTION] = g_param_spec_string ("description",
      "Description", "Short media description", NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_DURATION] = g_param_spec_uint64 ("duration",
      "Duration", "Media duration in seconds", 0, G_MAXUINT64, 0,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}

static void
gtuber_media_info_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  GtuberMediaInfo *self = GTUBER_MEDIA_INFO (object);

  switch (prop_id) {
    case PROP_ID:
      g_value_set_string (value, gtuber_media_info_get_id (self));
      break;
    case PROP_TITLE:
      g_value_set_string (value, gtuber_media_info_get_title (self));
      break;
    case PROP_DESCRIPTION:
      g_value_set_string (value, gtuber_media_info_get_description (self));
      break;
    case PROP_DURATION:
      g_value_set_uint64 (value, gtuber_media_info_get_duration (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gtuber_media_info_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  GtuberMediaInfo *self = GTUBER_MEDIA_INFO (object);

  switch (prop_id) {
    case PROP_ID:
      gtuber_media_info_set_id (self, g_value_get_string (value));
      break;
    case PROP_TITLE:
      gtuber_media_info_set_title (self, g_value_get_string (value));
      break;
    case PROP_DESCRIPTION:
      gtuber_media_info_set_description (self, g_value_get_string (value));
      break;
    case PROP_DURATION:
      gtuber_media_info_set_duration (self, g_value_get_uint64 (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gtuber_media_info_finalize (GObject *object)
{
  GtuberMediaInfo *self = GTUBER_MEDIA_INFO (object);

  g_debug ("Media info finalize");

  g_free (self->id);
  g_free (self->title);
  g_free (self->description);

  g_ptr_array_unref (self->streams);
  g_ptr_array_unref (self->adaptive_streams);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gtuber_media_info_get_id:
 * @info: a #GtuberMediaInfo
 *
 * Returns: (transfer none): Media ID or %NULL when undetermined.
 **/
const gchar *
gtuber_media_info_get_id (GtuberMediaInfo *self)
{
  g_return_val_if_fail (GTUBER_IS_MEDIA_INFO (self), NULL);

  return self->id;
}

/**
 * gtuber_media_info_set_id:
 * @info: a #GtuberMediaInfo
 * @id: media ID
 *
 * Sets the media ID.
 *
 * This is mainly useful for plugin development.
 **/
void
gtuber_media_info_set_id (GtuberMediaInfo *self, const gchar *id)
{
  g_return_if_fail (GTUBER_IS_MEDIA_INFO (self));

  g_free (self->id);
  self->id = g_strdup (id);
}

/**
 * gtuber_media_info_get_title:
 * @info: a #GtuberMediaInfo
 *
 * Returns: (transfer none): Media title or %NULL when undetermined.
 **/
const gchar *
gtuber_media_info_get_title (GtuberMediaInfo *self)
{
  g_return_val_if_fail (GTUBER_IS_MEDIA_INFO (self), NULL);

  return self->title;
}

/**
 * gtuber_media_info_set_description:
 * @info: a #GtuberMediaInfo
 * @description: media description
 *
 * Sets the media description.
 *
 * This is mainly useful for plugin development.
 **/
void
gtuber_media_info_set_description (GtuberMediaInfo *self, const gchar *description)
{
  g_return_if_fail (GTUBER_IS_MEDIA_INFO (self));

  g_free (self->description);
  self->description = g_strdup (description);
}

/**
 * gtuber_media_info_get_description:
 * @info: a #GtuberMediaInfo
 *
 * Returns: (transfer none): Media description or %NULL when undetermined.
 **/
const gchar *
gtuber_media_info_get_description (GtuberMediaInfo *self)
{
  g_return_val_if_fail (GTUBER_IS_MEDIA_INFO (self), NULL);

  return self->description;
}

/**
 * gtuber_media_info_set_title:
 * @info: a #GtuberMediaInfo
 * @title: media title
 *
 * Sets the media title.
 *
 * This is mainly useful for plugin development.
 **/
void
gtuber_media_info_set_title (GtuberMediaInfo *self, const gchar *title)
{
  g_return_if_fail (GTUBER_IS_MEDIA_INFO (self));

  g_free (self->title);
  self->title = g_strdup (title);
}

/**
 * gtuber_media_info_get_duration:
 * @info: a #GtuberMediaInfo
 *
 * Returns: Media duration in seconds or 0 when undetermined.
 **/
guint64
gtuber_media_info_get_duration (GtuberMediaInfo *self)
{
  g_return_val_if_fail (GTUBER_IS_MEDIA_INFO (self), 0);

  return self->duration;
}

/**
 * gtuber_media_info_set_duration:
 * @info: a #GtuberMediaInfo
 * @duration: media duration
 *
 * Sets the media duration in seconds.
 *
 * This is mainly useful for plugin development.
 **/
void
gtuber_media_info_set_duration (GtuberMediaInfo *self, guint64 duration)
{
  g_return_if_fail (GTUBER_IS_MEDIA_INFO (self));

  self->duration = duration;
}

/**
 * gtuber_media_info_get_streams:
 * @info: a #GtuberMediaInfo
 *
 * Returns: (transfer none) (element-type GtuberStream): A #GPtrArray of
 * available #GtuberStream instances.	
 */
const GPtrArray *
gtuber_media_info_get_streams (GtuberMediaInfo *self)
{
  g_return_val_if_fail (GTUBER_IS_MEDIA_INFO (self), NULL);

  return self->streams;
}

/**
 * gtuber_media_info_add_stream:
 * @info: a #GtuberMediaInfo
 * @stream: a #GtuberStream
 *
 * Add another #GtuberStream to the end of streams array.
 * This adds the stream pointer to the #GPtrArray. Do not free it afterwards.
 *
 * This is mainly useful for plugin development.
 */
void
gtuber_media_info_add_stream (GtuberMediaInfo *self, GtuberStream *stream)
{
  g_return_if_fail (GTUBER_IS_MEDIA_INFO (self));
  g_return_if_fail (GTUBER_IS_STREAM (stream));

  g_ptr_array_add (self->streams, stream);
}

/**
 * gtuber_media_info_get_adaptive_streams:
 * @info: a #GtuberMediaInfo
 *
 * Returns: (transfer none) (element-type GtuberAdaptiveStream): A #GPtrArray of
 * available #GtuberAdaptiveStream instances.
 */
const GPtrArray *
gtuber_media_info_get_adaptive_streams (GtuberMediaInfo *self)
{
  g_return_val_if_fail (GTUBER_IS_MEDIA_INFO (self), NULL);

  return self->adaptive_streams;
}

/**
 * gtuber_media_info_add_adaptive_stream:
 * @info: a #GtuberMediaInfo
 * @stream: a #GtuberAdaptiveStream
 *
 * Add another #GtuberAdaptiveStream to the end of adaptive streams array.
 * This adds the adaptive stream pointer to the #GPtrArray. Do not free it afterwards.
 *
 * This is mainly useful for plugin development.
 */
void
gtuber_media_info_add_adaptive_stream (GtuberMediaInfo *self, GtuberAdaptiveStream *stream)
{
  g_return_if_fail (GTUBER_IS_MEDIA_INFO (self));
  g_return_if_fail (GTUBER_IS_ADAPTIVE_STREAM (stream));

  g_ptr_array_add (self->adaptive_streams, stream);
}