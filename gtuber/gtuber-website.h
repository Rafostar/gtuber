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

#ifndef __GTUBER_WEBSITE_H__
#define __GTUBER_WEBSITE_H__

#if !defined(__GTUBER_INSIDE__) && !defined(GTUBER_COMPILATION)
#error "Only <gtuber/gtuber.h> and <gtuber/gtuber-plugin-devel.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>
#include <libsoup/soup.h>

#include <gtuber/gtuber-enums.h>
#include <gtuber/gtuber-media-info.h>

G_BEGIN_DECLS

#define GTUBER_TYPE_WEBSITE            (gtuber_website_get_type ())
#define GTUBER_IS_WEBSITE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTUBER_TYPE_WEBSITE))
#define GTUBER_IS_WEBSITE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTUBER_TYPE_WEBSITE))
#define GTUBER_WEBSITE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTUBER_TYPE_WEBSITE, GtuberWebsiteClass))
#define GTUBER_WEBSITE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTUBER_TYPE_WEBSITE, GtuberWebsite))
#define GTUBER_WEBSITE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTUBER_TYPE_WEBSITE, GtuberWebsiteClass))

#define GTUBER_WEBSITE_ERROR           (gtuber_website_error_quark ())

/**
 * GtuberWebsite:
 *
 * Plugin website base class.
 */
typedef struct _GtuberWebsite GtuberWebsite;
typedef struct _GtuberWebsiteClass GtuberWebsiteClass;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtuberWebsite, g_object_unref)
#endif

struct _GtuberWebsite
{
  GObject parent;

  gchar *uri;
};

/**
 * GtuberWebsiteClass:
 * @parent_class: The object class structure.
 * @handles_input_stream: When set to %TRUE, parse_input_stream() will be called
 *   at parsing stage otherwise parse_response() will be used.
 * @create_request: Create and pass #SoupMessage to send.
 * @parse_response: Read #SoupMessage response body and fill #GtuberMediaInfo.
 * @parse_input_stream: Read #GInputStream and fill #GtuberMediaInfo.
 */
struct _GtuberWebsiteClass
{
  GObjectClass parent_class;

  gboolean handles_input_stream;

  GtuberFlow (* create_request) (GtuberWebsite   *website,
                                 GtuberMediaInfo *info,
                                 SoupMessage    **msg,
                                 GError         **error);

  GtuberFlow (* parse_response) (GtuberWebsite   *website,
                                 gchar           *data,
                                 GtuberMediaInfo *info,
                                 GError         **error);

  GtuberFlow (* parse_input_stream) (GtuberWebsite   *website,
                                     GInputStream    *stream,
                                     GtuberMediaInfo *info,
                                     GError         **error);
};

GType         gtuber_website_get_type              (void);

const gchar * gtuber_website_get_uri               (GtuberWebsite *website);
void          gtuber_website_set_uri               (GtuberWebsite *website, const gchar *uri);

GQuark        gtuber_website_error_quark           (void);

G_END_DECLS

#endif /* __GTUBER_WEBSITE_H__ */
