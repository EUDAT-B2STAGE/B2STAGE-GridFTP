#ifndef PID_MANAGER_H_INCLUDED
#define PID_MANAGER_H_INCLUDED

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "cJSON.h"
#include "globus_gridftp_server.h"


int manage_pid(char *pid_handle_URL, char *PID, char **URL );

#endif
