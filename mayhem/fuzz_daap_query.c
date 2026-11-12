#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "parsers/daap_parser.h"

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  struct daap_result result;
  char *input;

  if (size > 4096)
    return -1;

  input = malloc(size + 1);
  if (!input)
    return -1;
  memcpy(input, data, size);
  input[size] = '\0';

  memset(&result, 0, sizeof(result));
  daap_lex_parse(&result, input);

  free(input);
  return 0;
}
