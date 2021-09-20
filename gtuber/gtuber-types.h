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

#ifndef __GTUBER_TYPES_H__
#define __GTUBER_TYPES_H__

#if !defined(__GTUBER_INSIDE__) && !defined(GTUBER_COMPILATION)
#error "Only <gtuber/gtuber.h> can be included directly."
#endif

#include <gtuber/gtuber-enum-types.h>

G_BEGIN_DECLS

/**
 * GtuberClient:
 *
 * Gtuber web client used to obtain media info.
 */
typedef struct _GtuberClient GtuberClient;
typedef struct _GtuberClientClass GtuberClientClass;

/**
 * GtuberWebsite:
 *
 * Plugin website base class.
 */
typedef struct _GtuberWebsite GtuberWebsite;
typedef struct _GtuberWebsiteClass GtuberWebsiteClass;

/**
 * GtuberStream:
 *
 * Contains values of peculiar media stream.
 */
typedef struct _GtuberStream GtuberStream;
typedef struct _GtuberStreamClass GtuberStreamClass;

/**
 * GtuberAdaptiveStream:
 *
 * Contains values of peculiar adaptive media stream.
 */
typedef struct _GtuberAdaptiveStream GtuberAdaptiveStream;
typedef struct _GtuberAdaptiveStreamClass GtuberAdaptiveStreamClass;

/**
 * GtuberMediaInfo:
 *
 * Contains result with parsed media info.
 */
typedef struct _GtuberMediaInfo GtuberMediaInfo;
typedef struct _GtuberMediaInfoClass GtuberMediaInfoClass;

/**
 * GtuberStreamMimeType:
 * @GTUBER_STREAM_MIME_TYPE_UNKNOWN: MIME type is not specified.
 * @GTUBER_STREAM_MIME_TYPE_VIDEO_MP4: video/mp4.
 * @GTUBER_STREAM_MIME_TYPE_VIDEO_WEBM: video/webm.
 * @GTUBER_STREAM_MIME_TYPE_AUDIO_MP4: audio/mp4.
 * @GTUBER_STREAM_MIME_TYPE_AUDIO_WEBM: audio/webm.
 */
typedef enum
{
  GTUBER_STREAM_MIME_TYPE_UNKNOWN = 0,
  GTUBER_STREAM_MIME_TYPE_VIDEO_MP4,
  GTUBER_STREAM_MIME_TYPE_AUDIO_MP4,
  GTUBER_STREAM_MIME_TYPE_VIDEO_WEBM,
  GTUBER_STREAM_MIME_TYPE_AUDIO_WEBM
} GtuberStreamMimeType;

/**
 * GtuberFlow:
 * @GTUBER_FLOW_OK: continue parsing.
 * @GTUBER_FLOW_ERROR: give up.
 * @GTUBER_FLOW_RETRY: retry from beginning.
 */
typedef enum
{
  GTUBER_FLOW_OK = 0,
  GTUBER_FLOW_ERROR,
  GTUBER_FLOW_RETRY,
} GtuberFlow;

/**
 * GtuberClientError:
 * @GTUBER_CLIENT_ERROR_NO_PLUGIN: none of the installed plugins could handle URI. 
 * @GTUBER_CLIENT_ERROR_MISSING_INFO: plugin did not fill the media info.
 */
typedef enum
{
  GTUBER_CLIENT_ERROR_NO_PLUGIN,
  GTUBER_CLIENT_ERROR_MISSING_INFO,
} GtuberClientError;

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

#endif /* __GTUBER_TYPES_H__ */
