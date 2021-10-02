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

#ifndef __GTUBER_MANIFEST_GENERATOR_H__
#define __GTUBER_MANIFEST_GENERATOR_H__

#if !defined(__GTUBER_INSIDE__) && !defined(GTUBER_COMPILATION)
#error "Only <gtuber/gtuber.h> can be included directly."
#endif

#include <glib-object.h>

#include <gtuber/gtuber-media-info.h>

G_BEGIN_DECLS

#define GTUBER_TYPE_MANIFEST_GENERATOR            (gtuber_manifest_generator_get_type ())
#define GTUBER_IS_MANIFEST_GENERATOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTUBER_TYPE_MANIFEST_GENERATOR))
#define GTUBER_IS_MANIFEST_GENERATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTUBER_TYPE_MANIFEST_GENERATOR))
#define GTUBER_MANIFEST_GENERATOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTUBER_TYPE_MANIFEST_GENERATOR, GtuberManifestGeneratorClass))
#define GTUBER_MANIFEST_GENERATOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTUBER_TYPE_MANIFEST_GENERATOR, GtuberManifestGenerator))
#define GTUBER_MANIFEST_GENERATOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTUBER_TYPE_MANIFEST_GENERATOR, GtuberManifestGeneratorClass))

#define GTUBER_MANIFEST_GENERATOR_ERROR           (gtuber_manifest_generator_error_quark ())

/**
 * GtuberManifestGenerator:
 *
 * Gtuber manifest generator
 */
typedef struct _GtuberManifestGenerator GtuberManifestGenerator;
typedef struct _GtuberManifestGeneratorClass GtuberManifestGeneratorClass;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtuberManifestGenerator, g_object_unref)
#endif

GType                     gtuber_manifest_generator_get_type               (void);

GtuberManifestGenerator * gtuber_manifest_generator_new                    (void);

void                      gtuber_manifest_generator_set_media_info         (GtuberManifestGenerator *gen, GtuberMediaInfo *info);

gchar *                   gtuber_manifest_generator_to_data                (GtuberManifestGenerator *gen);

gboolean                  gtuber_manifest_generator_to_file                (GtuberManifestGenerator *gen, const gchar *filename, GError **error);

GQuark                    gtuber_manifest_generator_error_quark            (void);

G_END_DECLS

#endif /* __GTUBER_MANIFEST_GENERATOR_H__ */
