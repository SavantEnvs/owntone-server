// Support code for the harnesses: globals that upstream defines in main.c (not linked here), and
// logger setup so DPRINTF output does not flood stderr on every input.
#include <event2/event.h>

#include "logger.h"

struct event_base *evbase_main;

int
LLVMFuzzerInitialize(int *argc, char ***argv)
{
  logger_init("/dev/null", NULL, E_FATAL, NULL);
  return 0;
}
