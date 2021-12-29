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
#include <gtuber/gtuber-cache.h>

G_BEGIN_DECLS

#define GTUBER_TYPE_WEBSITE            (gtuber_website_get_type ())
#define GTUBER_IS_WEBSITE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTUBER_TYPE_WEBSITE))
#define GTUBER_IS_WEBSITE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTUBER_TYPE_WEBSITE))
#define GTUBER_WEBSITE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTUBER_TYPE_WEBSITE, GtuberWebsiteClass))
#define GTUBER_WEBSITE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTUBER_TYPE_WEBSITE, GtuberWebsite))
#define GTUBER_WEBSITE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTUBER_TYPE_WEBSITE, GtuberWebsiteClass))

#define GTUBER_WEBSITE_ERROR           (gtuber_website_error_quark ())

/**
 * GTUBER_WEBSITE_PLUGIN_DECLARE:
 * @camel: camel case name of the website.
 * @lower: lowercase name of the website, with multiple words
 *   separated by `_`.
 * @upper: uppercase name of the website.
 *
 * Convenient macro to declare a new plugin that extends #GtuberWebsite.
 */
#define GTUBER_WEBSITE_PLUGIN_DECLARE(camel,lower,upper)                            \
G_DECLARE_FINAL_TYPE (Gtuber##camel, gtuber_##lower, GTUBER, upper, GtuberWebsite)  \
                                                                                    \
G_MODULE_EXPORT GtuberWebsite *plugin_query (GUri *uri);                            \
                                                                                    \
G_GNUC_UNUSED static inline Gtuber##camel * G_PASTE (gtuber_##lower, _new) (void) { \
    return g_object_new (G_PASTE (gtuber_##lower, _get_type) (), NULL); }           \
G_GNUC_UNUSED static inline gchar * G_PASTE (gtuber_##lower, _cache_read)           \
    (const gchar *key) {                                                            \
    return gtuber_cache_plugin_read (G_STRINGIFY (lower), key); }                   \
G_GNUC_UNUSED static inline void G_PASTE (gtuber_##lower, _cache_write)             \
    (const gchar *key, const gchar *val, gint64 exp) {                              \
    gtuber_cache_plugin_write (G_STRINGIFY (lower), key, val, exp); }               \
G_GNUC_UNUSED static inline void G_PASTE (gtuber_##lower, _cache_write_epoch)       \
    (const gchar *key, const gchar *val, gint64 epoch) {                            \
    gtuber_cache_plugin_write_epoch (G_STRINGIFY (lower), key, val, epoch); }

/**
 * GTUBER_WEBSITE_PLUGIN_DEFINE:
 * @camel: camel case name of the website.
 * @lower: lowercase name of the website, with multiple words
 *   separated by `_`.
 *
 * Convenient macro that wraps around G_DEFINE_TYPE with #GtuberWebsite.
 */
#define GTUBER_WEBSITE_PLUGIN_DEFINE(camel,lower)                                   \
G_DEFINE_TYPE (Gtuber##camel, gtuber_##lower, GTUBER_TYPE_WEBSITE)                  \

/**
 * GTUBER_WEBSITE_PLUGIN_EXPORT_SCHEMES:
 * @...: %NULL terminated list of supported schemes.
 *
 * Convenient macro that exports plugin supported schemes.
 */
#define GTUBER_WEBSITE_PLUGIN_EXPORT_SCHEMES(...)                                   \
static const gchar *_schemes_compat[] = { __VA_ARGS__ };                            \
G_MODULE_EXPORT const gchar *const *plugin_get_schemes (void);                      \
const gchar *const *plugin_get_schemes (void) {                                     \
    return _schemes_compat; }

/**
 * GTUBER_WEBSITE_PLUGIN_EXPORT_HOSTS:
 * @...: %NULL terminated list of supported hosts.
 *
 * Convenient macro that exports plugin supported hosts.
 */
#define GTUBER_WEBSITE_PLUGIN_EXPORT_HOSTS(...)                                     \
static const gchar *_hosts_compat[] = { __VA_ARGS__ };                              \
G_MODULE_EXPORT const gchar *const *plugin_get_hosts (void);                        \
const gchar *const *plugin_get_hosts (void) {                                       \
    return _hosts_compat; }

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
 * @prepare: If plugin needs to do some post init blocking IO (like reading cache)
 *   before it can be used, this is a good place to do so.
 * @create_request: Create and pass #SoupMessage to send.
 * @read_response: Use to check #SoupStatus and response #SoupMessageHeaders
 *   from send #SoupMessage.
 * @parse_data: Read data of response body and fill #GtuberMediaInfo.
 * @parse_input_stream: Read #GInputStream and fill #GtuberMediaInfo.
 * @set_user_req_headers: Set request headers for user. Default implementation
 *   will set them from last #SoupMessage, skipping some common and invalid ones.
 */
struct _GtuberWebsiteClass
{
  GObjectClass parent_class;

  gboolean handles_input_stream;

  void (* prepare) (GtuberWebsite *website);

  GtuberFlow (* create_request) (GtuberWebsite   *website,
                                 GtuberMediaInfo *info,
                                 SoupMessage    **msg,
                                 GError         **error);

  GtuberFlow (* read_response) (GtuberWebsite *website,
                                SoupMessage   *msg,
                                GError       **error);

  GtuberFlow (* parse_data) (GtuberWebsite   *website,
                             gchar           *data,
                             GtuberMediaInfo *info,
                             GError         **error);

  GtuberFlow (* parse_input_stream) (GtuberWebsite   *website,
                                     GInputStream    *stream,
                                     GtuberMediaInfo *info,
                                     GError         **error);

  GtuberFlow (* set_user_req_headers) (GtuberWebsite      *website,
                                       SoupMessageHeaders *req_headers,
                                       GHashTable         *user_headers,
                                       GError            **error);
};

GType         gtuber_website_get_type              (void);

const gchar * gtuber_website_get_uri               (GtuberWebsite *website);
void          gtuber_website_set_uri               (GtuberWebsite *website, const gchar *uri);

GQuark        gtuber_website_error_quark           (void);

G_END_DECLS

#endif /* __GTUBER_WEBSITE_H__ */
