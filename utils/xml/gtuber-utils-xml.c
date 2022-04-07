/*
 * Copyright (C) 2022 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#include <gio/gio.h>
#include <libxml/HTMLparser.h>

#include "gtuber-utils-xml.h"

static const gchar *
_find_property (xmlAttr *start_attr, const gchar *search_str)
{
  xmlAttr *attr;

  for (attr = start_attr; attr; attr = attr->next) {
    if (!strcmp ((gchar *) attr->name, search_str) && attr->children)
      return (const gchar *) attr->children->content;
  }

  return NULL;
}

static const gchar *
_iterate_nodes_for_prop (xmlNode *start_node, const gchar *search_str)
{
  xmlNode *node;
  const gchar *value = NULL;

  for (node = start_node; node; node = node->next) {
    if (node->type != XML_ELEMENT_NODE)
      continue;

    if ((value = _find_property (node->properties, search_str)))
      return value;

    if ((value = _iterate_nodes_for_prop (node->children, search_str)))
      return value;
  }

  return value;
}

static gchar *
_obtain_json_data (const xmlChar *content, const gchar *search_str)
{
  gchar **data, *value = NULL;
  guint i;

  if (!content)
    return NULL;

  data = g_strsplit ((const gchar *) content, "=", 0);

  for (i = 0; data[i]; i++) {
    g_strchomp (data[i]);

    if (g_str_has_suffix (data[i], search_str)) {
      if (data[i + 1]) {
        GString *string;

        g_strstrip (data[i + 1]);

        string = g_string_new (data[i + 1]);
        if (string->str[string->len - 1] == ';')
          g_string_truncate (string, string->len - 1);

        value = g_string_free (string, FALSE);
        break;
      }
    }
  }

  g_strfreev (data);

  return value;
}

static gchar *
_iterate_nodes_obtain_json (xmlNode *start_node, const gchar *search_str)
{
  xmlNode *node;
  gchar *value = NULL;

  for (node = start_node; node; node = node->next) {
    if (node->type != XML_ELEMENT_NODE && node->type != XML_CDATA_SECTION_NODE)
      continue;

    if ((value = _obtain_json_data (node->content, search_str)))
      return value;

    if ((value = _iterate_nodes_obtain_json (node->children, search_str)))
      return value;
  }

  return value;
}

xmlDoc *
gtuber_utils_xml_load_html_from_data (const gchar *data, GError **error)
{
  xmlDoc *doc;

  g_return_val_if_fail (data != NULL, NULL);

  g_debug ("Parsing HTML...");

  doc = htmlReadMemory (data, strlen (data), "gtuber.html", NULL,
      HTML_PARSE_RECOVER | HTML_PARSE_NOERROR);

  if (!doc && error) {
    g_set_error (error, G_IO_ERROR,
        G_IO_ERROR_FAILED,
        "Could not parse HTML data");
  }
  g_debug ("HTML parsing %s", doc ? "successful" : "failed");

  return doc;
}

const gchar *
gtuber_utils_xml_get_property_content (xmlDoc *doc, const gchar *name)
{
  xmlNode *root = xmlDocGetRootElement (doc);
  const gchar *value;

  g_debug ("Node property search: %s", name);
  value = _iterate_nodes_for_prop (root, name);
  g_debug ("Found value: %s", value);

  return value;
}

gchar *
gtuber_utils_xml_obtain_json_in_node (xmlDoc *doc, const gchar *json_name)
{
  xmlNode *root = xmlDocGetRootElement (doc);
  gchar *value;

  g_debug ("Node JSON search: %s", json_name);
  value = _iterate_nodes_obtain_json (root, json_name);
  g_debug ("Found value: %s", value);

  return value;
}
