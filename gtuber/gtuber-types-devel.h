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

#ifndef __GTUBER_TYPES_DEVEL_H__
#define __GTUBER_TYPES_DEVEL_H__

#if !defined(__GTUBER_PLUGIN_DEVEL_INSIDE__) && !defined(GTUBER_COMPILATION)
#error "Only "gtuber/gtuber-plugin-devel.h" can be included directly."
#endif

G_BEGIN_DECLS

/**
 * GtuberWebsite:
 *
 * Plugin website base class.
 */
typedef struct _GtuberWebsite GtuberWebsite;
typedef struct _GtuberWebsiteClass GtuberWebsiteClass;

/**
 * GtuberWebsiteError:
 * @GTUBER_WEBSITE_ERROR_REQUEST_CREATE_FAILED: could not create soup message.
 * @GTUBER_WEBSITE_ERROR_PARSE_FAILED: parsing of website response failed.
 * @GTUBER_WEBSITE_ERROR_OTHER: some other error occured.
 */
typedef enum
{
  GTUBER_WEBSITE_ERROR_REQUEST_CREATE_FAILED,
  GTUBER_WEBSITE_ERROR_PARSE_FAILED,
  GTUBER_WEBSITE_ERROR_OTHER,
} GtuberWebsiteError;

/**
 * GtuberUtilsError:
 * @GTUBER_UTILS_ERROR_COMMON: common error when one of the utils functions fails.
 */
typedef enum
{
  GTUBER_UTILS_ERROR_COMMON,
} GtuberUtilsError;

G_END_DECLS

#endif /* __GTUBER_TYPES_DEVEL_H__ */
