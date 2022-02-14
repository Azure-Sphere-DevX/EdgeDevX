#pragma once

#include "dx_exit_codes.h"
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <uv.h>

int dx_getTerminationExitCode(void);
void dx_eventLoopRun(void);
void dx_registerTerminationHandler(void);
void dx_terminate(int exitCode);
