/*
 * Copyright (C) 2022 Rafał Dzięgiel <rafostar.github@gmail.com>
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

  data = g_strsplit ((const gchar *) content, search_str, 0);

  /* Start after first occurence */
  for (i = 1; i < g_strv_length (data); i++) {
    const gchar *text = data[i];
    gsize index, open_index = 0;
    gint open_brackets = 0;
    gboolean was_open = FALSE, found = FALSE;

    /* Find index of last closing bracket */
    for (index = 0; index < strlen (text); index++) {
      if (text[index] == '{') {
        if (!was_open) {
          was_open = TRUE;
          open_index = index;
        }
        open_brackets++;
      } else if (was_open && text[index] == '}') {
        open_brackets--;
      }

      if ((found = was_open && open_brackets == 0))
        break;
    }

    if (found) {
      GString *string = g_string_new (text);

      /* Create our substring */
      g_string_truncate (string, index + 1);
      if (open_index > 0)
        g_string_erase (string, 0, open_index);

      value = g_string_free (string, FALSE);
      break;
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
