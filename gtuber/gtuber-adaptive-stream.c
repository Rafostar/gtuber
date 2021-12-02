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
 * SECTION:gtuber-adaptive-stream
 * @title: GtuberAdaptiveStream
 * @short_description: represents an adaptive media stream,
 *  usually for use in an adaptive streaming manifest
 */

/**
 * SECTION:gtuber-adaptive-stream-devel
 * @title: GtuberAdaptiveStream Development
 */

#include "gtuber-stream.h"
#include "gtuber-stream-private.h"
#include "gtuber-adaptive-stream.h"
#include "gtuber-adaptive-stream-devel.h"
#include "gtuber-adaptive-stream-private.h"

enum
{
  PROP_0,
  PROP_MANIFEST_TYPE,
  PROP_LAST
};

#define parent_class gtuber_adaptive_stream_parent_class
G_DEFINE_TYPE (GtuberAdaptiveStream, gtuber_adaptive_stream, GTUBER_TYPE_STREAM);

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void gtuber_adaptive_stream_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec);

static void
gtuber_adaptive_stream_init (GtuberAdaptiveStream *self)
{
  self = gtuber_adaptive_stream_get_instance_private (self);

  self->manifest_type = GTUBER_ADAPTIVE_STREAM_MANIFEST_UNKNOWN;

  self->init_start = 0;
  self->init_end = 0;

  self->index_start = 0;
  self->index_end = 0;
}

static void
gtuber_adaptive_stream_class_init (GtuberAdaptiveStreamClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->get_property = gtuber_adaptive_stream_get_property;

  param_specs[PROP_MANIFEST_TYPE] = g_param_spec_enum ("manifest-type",
      "Adaptive Stream Manifest Type", "The manifest type adaptive stream belongs to",
      GTUBER_TYPE_ADAPTIVE_STREAM_MANIFEST, GTUBER_ADAPTIVE_STREAM_MANIFEST_UNKNOWN,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}

static void
gtuber_adaptive_stream_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  GtuberAdaptiveStream *self = GTUBER_ADAPTIVE_STREAM (object);

  switch (prop_id) {
    case PROP_MANIFEST_TYPE:
      g_value_set_enum (value, gtuber_adaptive_stream_get_manifest_type (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * gtuber_adaptive_stream_new:
 *
 * Creates a new #GtuberAdaptiveStream instance.
 *
 * This is mainly useful for plugin development.
 *
 * Returns: (transfer full): a new #GtuberAdaptiveStream instance.
 */
GtuberAdaptiveStream *
gtuber_adaptive_stream_new (void)
{
  return g_object_new (GTUBER_TYPE_ADAPTIVE_STREAM, NULL);
}

/**
 * gtuber_adaptive_stream_get_manifest_type:
 * @stream: a #GtuberAdaptiveStream
 *
 * Returns: a #GtuberAdaptiveStreamManifest representing
 *   type of the manifest adaptive stream belongs to.
 */
GtuberAdaptiveStreamManifest
gtuber_adaptive_stream_get_manifest_type (GtuberAdaptiveStream *self)
{
  g_return_val_if_fail (GTUBER_IS_ADAPTIVE_STREAM (self),
      GTUBER_ADAPTIVE_STREAM_MANIFEST_UNKNOWN);

  return self->manifest_type;
}

/**
 * gtuber_adaptive_stream_set_manifest_type:
 * @stream: a #GtuberAdaptiveStream
 * @type: a #GtuberAdaptiveStreamManifest
 *
 * Sets the adaptive stream manifest type.
 *
 * This is mainly useful for plugin development.
 */
void
gtuber_adaptive_stream_set_manifest_type (GtuberAdaptiveStream *self,
    GtuberAdaptiveStreamManifest type)
{
  g_return_if_fail (GTUBER_IS_ADAPTIVE_STREAM (self));

  self->manifest_type = type;
}

/**
 * gtuber_adaptive_stream_get_init_range:
 * @stream: a #GtuberAdaptiveStream
 * @start: (out) (optional): the start of stream init range
 * @end: (out) (optional): the end of stream init range
 *
 * Gets the byte range of stream initialization segment.
 *
 * Returns: %TRUE if successful, with the out parameters set, %FALSE otherwise.
 */
gboolean
gtuber_adaptive_stream_get_init_range (GtuberAdaptiveStream *self,
    guint64 *start, guint64 *end)
{
  g_return_val_if_fail (GTUBER_IS_ADAPTIVE_STREAM (self), FALSE);

  if (self->init_end <= self->init_start)
    return FALSE;

  if (start)
    *start = self->init_start;
  if (end)
    *end = self->init_end;

  return TRUE;
}

/**
 * gtuber_adaptive_stream_set_init_range:
 * @stream: a #GtuberAdaptiveStream
 * @start: the start of stream init range
 * @end: the end of stream init range
 *
 * Sets the byte range of stream initialization segment.
 */
void
gtuber_adaptive_stream_set_init_range (GtuberAdaptiveStream *self,
    guint64 start, guint64 end)
{
  g_return_if_fail (GTUBER_IS_ADAPTIVE_STREAM (self));

  self->init_start = start;
  self->init_end = end;
}

/**
 * gtuber_adaptive_stream_get_index_range:
 * @stream: a #GtuberAdaptiveStream
 * @start: (out) (optional): the start of stream index range
 * @end: (out) (optional): the end of stream index range
 *
 * Gets the byte range of stream media segment.
 *
 * Returns: %TRUE if successful, with the out parameters set, %FALSE otherwise.
 */
gboolean
gtuber_adaptive_stream_get_index_range (GtuberAdaptiveStream *self,
    guint64 *start, guint64 *end)
{
  g_return_val_if_fail (GTUBER_IS_ADAPTIVE_STREAM (self), FALSE);

  if (self->index_end <= self->index_start)
    return FALSE;

  if (start)
    *start = self->index_start;
  if (end)
    *end = self->index_end;

  return TRUE;
}

/**
 * gtuber_adaptive_stream_set_index_range:
 * @stream: a #GtuberAdaptiveStream
 * @start: the start of stream index range
 * @end: the end of stream index range
 *
 * Sets the byte range of stream media segment.
 */
void
gtuber_adaptive_stream_set_index_range (GtuberAdaptiveStream *self,
    guint64 start, guint64 end)
{
  g_return_if_fail (GTUBER_IS_ADAPTIVE_STREAM (self));

  self->index_start = start;
  self->index_end = end;
}
