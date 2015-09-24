//pid_manager test class
#include <stdio.h>
#include "pid_manager.h"

int main()
{
  printf("Starting pid_manager test..\n");
  char handle_url[] = "http://hdl.handle.net/api/handles/";
  char PID[] = "/11100/0beb6af8-cbe5-11e3-a9da-e41f13eb41b2";
  char *URL;
  int res =  manage_pid(handle_url, PID, &URL);
  if (res == 0)
  {
    printf("Returned URL: %s\n", URL);
  }

  //free(URL);
  return 0;
}
