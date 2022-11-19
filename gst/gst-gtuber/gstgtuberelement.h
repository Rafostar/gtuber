/*
 * Copyright (C) 2021 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <gst/gst.h>

#define GST_GTUBER_HEADERS     "http-headers"
#define GST_GTUBER_REQ_HEADERS "request-headers"
#define GST_GTUBER_HEADER_UA   "User-Agent"

G_BEGIN_DECLS

GST_ELEMENT_REGISTER_DECLARE (gtubersrc);
GST_ELEMENT_REGISTER_DECLARE (gtuberuridemux);
GST_ELEMENT_REGISTER_DECLARE (gtuberdashdemux);
GST_ELEMENT_REGISTER_DECLARE (gtuberhlsdemux);

void gst_gtuber_element_init (GstPlugin *plugin);
void gst_gtuber_uri_handler_do_init (GType type);

G_END_DECLS
