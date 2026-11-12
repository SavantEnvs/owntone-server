#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pair_ap/pair-tlv.h"

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  pair_tlv_values_t *values;
  uint8_t *out;
  size_t out_size;
  int i;

  if (size > 8192)
    return -1;

  values = pair_tlv_new();
  if (!values)
    return -1;

  if (pair_tlv_parse(data, size, values) == 0)
    {
      for (i = 0; i < 20; i++)
	pair_tlv_get_value(values, i);

      out_size = 2 * size + 64;
      out = malloc(out_size);
      if (out)
	{
	  pair_tlv_format(values, out, &out_size);
	  free(out);
	}
    }

  pair_tlv_free(values);
  return 0;
}
