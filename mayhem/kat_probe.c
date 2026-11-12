// Known-answer probe over OwnTone's query parsers, TLV codec and XML wrapper (used by test.sh).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parsers/daap_parser.h"
#include "parsers/rsp_parser.h"
#include "parsers/mpd_parser.h"
#include "smartpl_query.h"
#include "pair_ap/pair-tlv.h"
#include "misc_xml.h"

static int
hexval(char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return -1;
}

int
main(int argc, char **argv)
{
  const char *kind = argc > 1 ? argv[1] : "";
  const char *in = argc > 2 ? argv[2] : "";

  if (!strcmp(kind, "daap"))
    {
      struct daap_result r;
      int ret;
      memset(&r, 0, sizeof(r));
      ret = daap_lex_parse(&r, in);
      printf("%d|%s\n", ret, ret == 0 ? r.str : "");
    }
  else if (!strcmp(kind, "rsp"))
    {
      struct rsp_result r;
      int ret;
      memset(&r, 0, sizeof(r));
      ret = rsp_lex_parse(&r, in);
      printf("%d|%s\n", ret, ret == 0 ? r.str : "");
    }
  else if (!strcmp(kind, "mpd"))
    {
      struct mpd_result r;
      int ret;
      memset(&r, 0, sizeof(r));
      ret = mpd_lex_parse(&r, in);
      printf("%d|%s|%s|%s|%d|%d\n", ret, ret == 0 && r.where ? r.where : "", ret == 0 && r.order ? r.order : "",
	     ret == 0 && r.group ? r.group : "", ret == 0 ? r.limit : 0, ret == 0 ? r.offset : 0);
    }
  else if (!strcmp(kind, "smartpl"))
    {
      struct smartpl s;
      int ret;
      memset(&s, 0, sizeof(s));
      ret = smartpl_query_parse_string(&s, in);
      printf("%d|%s|%s|%s|%s|%d\n", ret, ret == 0 ? s.title : "", ret == 0 && s.query_where ? s.query_where : "",
	     ret == 0 && s.having ? s.having : "", ret == 0 && s.order ? s.order : "", ret == 0 ? s.limit : 0);
      if (ret == 0)
	free_smartpl(&s, 1);
    }
  else if (!strcmp(kind, "tlv"))
    {
      // input: hex bytes; output: "type:hexvalue" per TLV in chain order, then re-encoded hex
      size_t n = strlen(in) / 2;
      uint8_t *buf = malloc(n + 1);
      uint8_t *out = malloc(2 * n + 64);
      size_t out_size = 2 * n + 64;
      pair_tlv_values_t *v = pair_tlv_new();
      size_t i;
      pair_tlv_t *t;
      int ret;

      for (i = 0; i < n; i++)
	buf[i] = (hexval(in[2 * i]) << 4) | hexval(in[2 * i + 1]);
      ret = pair_tlv_parse(buf, n, v);
      printf("%d", ret);
      if (ret == 0)
	{
	  for (t = v->head; t; t = t->next)
	    {
	      printf("|%u:", t->type);
	      for (i = 0; i < t->size; i++)
		printf("%02x", t->value[i]);
	    }
	  ret = pair_tlv_format(v, out, &out_size);
	  printf("|fmt%d:", ret);
	  for (i = 0; ret == 0 && i < out_size; i++)
	    printf("%02x", out[i]);
	}
      printf("\n");
      pair_tlv_free(v);
      free(buf);
      free(out);
    }
  else if (!strcmp(kind, "xml"))
    {
      // input: xml document; argv[3] = path; prints the value found there
      xml_node *top = xml_from_string(in);
      const char *path = argc > 3 ? argv[3] : "";
      const char *val;
      if (!top)
	{
	  printf("parse-error\n");
	  return 0;
	}
      val = xml_get_val(top, path);
      printf("%s\n", val ? val : "(null)");
      xml_free(top);
    }
  else
    {
      fprintf(stderr, "usage: kat_probe daap|rsp|mpd|smartpl|tlv|xml <input> [path]\n");
      return 2;
    }

  return 0;
}
