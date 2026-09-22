#pragma once

#include <stdatomic.h>

/* Shared flag — set to 0 by a successful /install or /exit, read by the main loop. */
extern atomic_int http_keep_running;

/* Set to 1 only when /install succeeds so shutdown knows to notify success. */
extern atomic_int install_completed;

/* Starts the HTTP server (blocking accept loop, spawns a thread per connection). */
int http_server_run(void);

/* Stops the server by closing the listening socket from another thread. */
void http_server_stop(void);