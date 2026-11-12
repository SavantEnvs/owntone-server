#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/tree.h>

#include "misc_xml.h"

#define MAX_NODES 512
#define MAX_DEPTH 32

// Walks the parsed tree and queries every element through the wrapper with the path that leads to it,
// the way OwnTone's callers (lastfm, pipe, rssscanner) address nodes, then rebuilds the tree with the
// wrapper's node builders, as httpd_rsp does for its replies.
static void
query(xml_node *top, xmlNode *n, const char *parent_path, int depth, int *budget)
{
  xmlNode *c;
  xmlAttr *a;
  char *path;
  size_t len;

  for (; n && *budget > 0; n = n->next)
    {
      if (n->type != XML_ELEMENT_NODE)
	continue;

      (*budget)--;

      len = strlen(parent_path) + strlen((const char *)n->name) + 2;
      path = malloc(len);
      if (!path)
	return;
      snprintf(path, len, "%s%s%s", parent_path, *parent_path ? "/" : "", (const char *)n->name);

      xml_get_node(top, path);
      xml_get_val(top, path);
      xml_get_attr(top, path, "id");
      for (a = n->properties; a; a = a->next)
	xml_get_attr(top, path, (const char *)a->name);
      xml_get_next(top, n);

      if (depth < MAX_DEPTH)
	query(top, n->children, path, depth + 1, budget);

      free(path);
    }
}

static void
build(xml_node *parent, xmlNode *n, int depth, int *budget)
{
  xml_node *node;
  const char *val;

  for (; n && *budget > 0; n = n->next)
    {
      if (n->type == XML_TEXT_NODE && n->content)
	{
	  (*budget)--;
	  xml_new_text(parent, (const char *)n->content);
	  continue;
	}
      if (n->type != XML_ELEMENT_NODE)
	continue;

      (*budget)--;
      val = (n->children && n->children->type == XML_TEXT_NODE) ? (const char *)n->children->content : NULL;
      if (depth & 1)
	node = xml_new_node(parent, (const char *)n->name, val);
      else
	node = xml_new_node_textf(parent, (const char *)n->name, "%s", val ? val : "");
      if (node && depth < MAX_DEPTH)
	build(node, n->children, depth + 1, budget);
    }
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  xml_node *top;
  xml_node *copy;
  xmlNode *root;
  char *input;
  char *out;
  int budget;

  if (size > 16384)
    return -1;

  input = malloc(size + 1);
  if (!input)
    return -1;
  memcpy(input, data, size);
  input[size] = '\0';

  top = xml_from_string(input);
  free(input);
  if (!top)
    return 0;

  root = top;

  budget = MAX_NODES;
  query(top, root, "", 0, &budget);

  out = xml_to_string(top, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
  free(out);
  out = xml_to_string(top, NULL);
  free(out);

  copy = xml_new_node(NULL, (const char *)root->name, NULL);
  if (copy)
    {
      budget = MAX_NODES;
      build(copy, root->children, 0, &budget);
      out = xml_to_string(copy, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
      free(out);
      xml_free(copy);
    }

  xml_free(top);
  return 0;
}
