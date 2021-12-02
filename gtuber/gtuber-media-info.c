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
 * SECTION:gtuber-media-info
 * @title: GtuberMediaInfo
 * @short_description: contains media information
 */

/**
 * SECTION:gtuber-media-info-devel
 * @title: GtuberMediaInfo Development
 */

#include "gtuber-media-info.h"
#include "gtuber-media-info-devel.h"
#include "gtuber-stream-private.h"
#include "gtuber-adaptive-stream-private.h"

enum
{
  PROP_0,
  PROP_ID,
  PROP_TITLE,
  PROP_DESCRIPTION,
  PROP_DURATION,
  PROP_HAS_STREAMS,
  PROP_HAS_ADAPTIVE_STREAMS,
  PROP_LAST
};

struct _GtuberMediaInfo
{
  GObject parent;

  gchar *id;
  gchar *title;
  gchar *description;
  guint duration;

  GPtrArray *streams;
  GPtrArray *adaptive_streams;

  GHashTable *chapters;
  GHashTable *req_headers;
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

  self->chapters =
      g_hash_table_new_full ((GHashFunc) g_direct_hash, (GEqualFunc) g_direct_equal,
          NULL, (GDestroyNotify) g_free);
  self->req_headers =
      g_hash_table_new_full ((GHashFunc) g_str_hash, (GEqualFunc) g_str_equal,
          (GDestroyNotify) g_free, (GDestroyNotify) g_free);
}

static void
gtuber_media_info_class_init (GtuberMediaInfoClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->get_property = gtuber_media_info_get_property;
  gobject_class->finalize = gtuber_media_info_finalize;

  param_specs[PROP_ID] = g_param_spec_string ("id",
      "ID", "The ID of media", NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_TITLE] = g_param_spec_string ("title",
      "Title", "Media title", NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_DESCRIPTION] = g_param_spec_string ("description",
      "Description", "Short media description", NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_DURATION] = g_param_spec_uint ("duration",
      "Duration", "Media duration in seconds", 0, G_MAXUINT, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_HAS_STREAMS] =
      g_param_spec_boolean ("has-streams", "Has Streams",
      "Check if media info has any normal streams",
      FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_HAS_ADAPTIVE_STREAMS] =
      g_param_spec_boolean ("has-adaptive-streams", "Has Adaptive Streams",
      "Check if media info has any adaptive streams",
      FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

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
      g_value_set_uint (value, gtuber_media_info_get_duration (self));
      break;
    case PROP_HAS_STREAMS:
      g_value_set_boolean (value, gtuber_media_info_get_has_streams (self));
      break;
    case PROP_HAS_ADAPTIVE_STREAMS:
      g_value_set_boolean (value, gtuber_media_info_get_has_adaptive_streams (self));
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

  g_hash_table_unref (self->chapters);
  g_hash_table_unref (self->req_headers);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gtuber_media_info_get_id:
 * @info: a #GtuberMediaInfo
 *
 * Returns: (transfer none): media ID or %NULL when undetermined.
 */
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
 */
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
 * Returns: (transfer none): media title or %NULL when undetermined.
 */
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
 */
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
 * Returns: (transfer none): media description or %NULL when undetermined.
 */
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
 */
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
 * Returns: media duration in seconds or 0 when undetermined.
 */
guint
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
 */
void
gtuber_media_info_set_duration (GtuberMediaInfo *self, guint duration)
{
  g_return_if_fail (GTUBER_IS_MEDIA_INFO (self));

  self->duration = duration;
}

/**
 * gtuber_media_info_insert_chapter:
 * @info: a #GtuberMediaInfo
 * @start: time in milliseconds when this chapter starts.
 * @name: name of the chapter.
 *
 * Inserts a new chapter to chapters #GHashTable. If a chapter with
 *   given time already exists, it will be replaced with the new one.
 *
 * This is mainly useful for plugin development.
 */
void
gtuber_media_info_insert_chapter (GtuberMediaInfo *self, guint64 start, const gchar *name)
{
  g_return_if_fail (GTUBER_IS_MEDIA_INFO (self));
  g_return_if_fail (name != NULL);

  g_hash_table_insert (self->chapters, GINT_TO_POINTER (start), g_strdup (name));
}

/**
 * gtuber_media_info_get_chapters:
 * @info: a #GtuberMediaInfo
 *
 * Get a #GHashTable with chapter start time and name pairs.
 *
 * Returns: (transfer none): a #GHashTable with chapters, or %NULL when none.
 */
GHashTable *
gtuber_media_info_get_chapters (GtuberMediaInfo *self)
{
  g_return_val_if_fail (GTUBER_IS_MEDIA_INFO (self), NULL);

  return self->chapters;
}

/**
 * gtuber_media_info_get_has_streams:
 * @info: a #GtuberMediaInfo
 *
 * Returns: %TRUE if info has streams, %FALSE otherwise.
 */
gboolean
gtuber_media_info_get_has_streams (GtuberMediaInfo *self)
{
  g_return_val_if_fail (GTUBER_IS_MEDIA_INFO (self), FALSE);

  return (self->streams && self->streams->len);
}

/**
 * gtuber_media_info_get_streams:
 * @info: a #GtuberMediaInfo
 *
 * Get a #GPtrArray of #GtuberStream instances. When no streams are available,
 *   an empty array is returned. Use gtuber_media_info_get_has_streams()
 *   to check if array will be empty.
 *
 * Returns: (transfer none) (element-type GtuberStream): a #GPtrArray of
 *   available #GtuberStream instances.
 */
GPtrArray *
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
 *   This adds the stream pointer to the #GPtrArray. Do not free it afterwards.
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
 * gtuber_media_info_get_has_adaptive_streams:
 * @info: a #GtuberMediaInfo
 *
 * Returns: %TRUE if info has adaptive streams, %FALSE otherwise.
 */
gboolean
gtuber_media_info_get_has_adaptive_streams (GtuberMediaInfo *self)
{
  g_return_val_if_fail (GTUBER_IS_MEDIA_INFO (self), FALSE);

  return (self->adaptive_streams && self->adaptive_streams->len);
}

/**
 * gtuber_media_info_get_adaptive_streams:
 * @info: a #GtuberMediaInfo
 *
 * Get a #GPtrArray of #GtuberAdaptiveStream instances. When no adaptive streams are
 *   available, an empty array is returned. Use gtuber_media_info_get_has_adaptive_streams()
 *   to check if array will be empty.
 *
 * Returns: (transfer none) (element-type GtuberAdaptiveStream): a #GPtrArray of
 *   available #GtuberAdaptiveStream instances.
 */
GPtrArray *
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
 *   This adds the adaptive stream pointer to the #GPtrArray. Do not free it afterwards.
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

/**
 * gtuber_media_info_get_request_headers:
 * @info: a #GtuberMediaInfo
 *
 * Get a #GHashTable with request headers name and key pairs.
 *
 * Users should use those headers for any future HTTP requests
 * to URIs within specific #GtuberMediaInfo object.
 *
 * Returns: (transfer none): a #GHashTable with recommended request headers.
 */
GHashTable *
gtuber_media_info_get_request_headers (GtuberMediaInfo *self)
{
  g_return_val_if_fail (GTUBER_IS_MEDIA_INFO (self), NULL);

  return self->req_headers;
}
