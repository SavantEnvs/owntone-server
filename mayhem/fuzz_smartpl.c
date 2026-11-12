#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "smartpl_query.h"

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  struct smartpl smartpl;
  char *input;

  if (size > 4096)
    return -1;

  input = malloc(size + 1);
  if (!input)
    return -1;
  memcpy(input, data, size);
  input[size] = '\0';

  memset(&smartpl, 0, sizeof(smartpl));
  if (smartpl_query_parse_string(&smartpl, input) == 0)
    free_smartpl(&smartpl, 1);

  free(input);
  return 0;
}
