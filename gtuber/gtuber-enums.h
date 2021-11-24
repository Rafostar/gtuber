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

#ifndef __GTUBER_ENUMS_H__
#define __GTUBER_ENUMS_H__

/**
 * SECTION:gtuber-enums
 * @title: Enumeration Types
 */

#if !defined(__GTUBER_INSIDE__) && !defined(GTUBER_COMPILATION)
#error "Only <gtuber/gtuber.h> and <gtuber/gtuber-plugin-devel.h> can be included directly."
#endif

#include <gtuber/gtuber-enum-types.h>

G_BEGIN_DECLS

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
 * GtuberCodecFlags:
 * @GTUBER_CODEC_UNKNOWN_VIDEO: an unknown video codec.
 * @GTUBER_CODEC_AVC: AVC video codec (usually H.264).
 * @GTUBER_CODEC_HEVC: HEVC video codec.
 * @GTUBER_CODEC_VP9: VP9 video codec.
 * @GTUBER_CODEC_AV1: AV1 video codec.
 * @GTUBER_CODEC_UNKNOWN_AUDIO: an unknown audio codec.
 * @GTUBER_CODEC_MP4A: audio codec in a MP4A format (usually AAC).
 * @GTUBER_CODEC_OPUS: Opus audio codec.
 */
typedef enum
{
  GTUBER_CODEC_UNKNOWN_VIDEO = (1 << 0),
  GTUBER_CODEC_AVC           = (1 << 1),
  GTUBER_CODEC_HEVC          = (1 << 2),
  GTUBER_CODEC_VP9           = (1 << 3),
  GTUBER_CODEC_AV1           = (1 << 4),

  GTUBER_CODEC_UNKNOWN_AUDIO = (1 << 10),
  GTUBER_CODEC_MP4A          = (1 << 11),
  GTUBER_CODEC_OPUS          = (1 << 12),
} GtuberCodecFlags;

/**
 * GtuberAdaptiveStreamManifest:
 * @GTUBER_ADAPTIVE_STREAM_MANIFEST_UNKNOWN: adaptive stream belongs to a manifest which type is unknown.
 * @GTUBER_ADAPTIVE_STREAM_MANIFEST_DASH: adaptive stream belongs to a DASH manifest.
 * @GTUBER_ADAPTIVE_STREAM_MANIFEST_HLS: adaptive stream belongs to a HLS manifest.
 */
typedef enum
{
  GTUBER_ADAPTIVE_STREAM_MANIFEST_UNKNOWN = 0,
  GTUBER_ADAPTIVE_STREAM_MANIFEST_DASH,
  GTUBER_ADAPTIVE_STREAM_MANIFEST_HLS
} GtuberAdaptiveStreamManifest;

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
 * GtuberManifestGeneratorError:
 * @GTUBER_MANIFEST_GENERATOR_ERROR_NO_DATA: no data was generated.
 */
typedef enum
{
  GTUBER_MANIFEST_GENERATOR_ERROR_NO_DATA,
} GtuberManifestGeneratorError;

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
 * GtuberFlow:
 * @GTUBER_FLOW_OK: continue parsing.
 * @GTUBER_FLOW_ERROR: give up.
 * @GTUBER_FLOW_RESTART: start from beginning.
 */
typedef enum
{
  GTUBER_FLOW_OK = 0,
  GTUBER_FLOW_ERROR,
  GTUBER_FLOW_RESTART,
} GtuberFlow;

G_END_DECLS

#endif /* __GTUBER_ENUMS_H__ */
