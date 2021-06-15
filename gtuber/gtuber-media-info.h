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

#ifndef __GTUBER_MEDIA_INFO_H__
#define __GTUBER_MEDIA_INFO_H__

#if !defined(__GTUBER_INSIDE__) && !defined(GTUBER_COMPILATION)
#error "Only <gtuber/gtuber.h> can be included directly."
#endif

#include <glib-object.h>
#include <gtuber/gtuber-types.h>

G_BEGIN_DECLS

#define GTUBER_TYPE_MEDIA_INFO            (gtuber_media_info_get_type ())
#define GTUBER_IS_MEDIA_INFO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTUBER_TYPE_MEDIA_INFO))
#define GTUBER_IS_MEDIA_INFO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTUBER_TYPE_MEDIA_INFO))
#define GTUBER_MEDIA_INFO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTUBER_TYPE_MEDIA_INFO, GtuberMediaInfoClass))
#define GTUBER_MEDIA_INFO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTUBER_TYPE_MEDIA_INFO, GtuberMediaInfo))
#define GTUBER_MEDIA_INFO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTUBER_TYPE_MEDIA_INFO, GtuberMediaInfoClass))
#define GTUBER_MEDIA_INFO_CAST(obj)       ((GtuberMediaInfo*)(obj))

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtuberMediaInfo, g_object_unref)
#endif

GType gtuber_media_info_get_type                            (void);

const gchar *     gtuber_media_info_get_id                  (GtuberMediaInfo *info);
void              gtuber_media_info_set_id                  (GtuberMediaInfo *info, const gchar *id);

const gchar *     gtuber_media_info_get_title               (GtuberMediaInfo *info);
void              gtuber_media_info_set_title               (GtuberMediaInfo *info, const gchar *title);

const gchar *     gtuber_media_info_get_description         (GtuberMediaInfo *info);
void              gtuber_media_info_set_description         (GtuberMediaInfo *info, const gchar *description);

guint64           gtuber_media_info_get_duration            (GtuberMediaInfo *info);
void              gtuber_media_info_set_duration            (GtuberMediaInfo *info, guint64 duration);

const GPtrArray * gtuber_media_info_get_streams             (GtuberMediaInfo *info);
void              gtuber_media_info_add_stream              (GtuberMediaInfo *info, GtuberStream *stream);

const GPtrArray * gtuber_media_info_get_adaptive_streams    (GtuberMediaInfo *info);
void              gtuber_media_info_add_adaptive_stream     (GtuberMediaInfo *info, GtuberAdaptiveStream *stream);

G_END_DECLS

#endif /* __GTUBER_MEDIA_INFO_H__ */
