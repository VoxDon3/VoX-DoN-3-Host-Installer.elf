/*
 * Minimal threaded HTTP server using raw BSD sockets (no libmicrohttpd).
 *
 * Listens on 127.0.0.1:<port>, accepts connections, spawns a detached thread
 * per connection, parses the request line, and serves entries from the
 * generated file registry (inflating raw-DEFLATE entries via puff.c on the
 * fly). Special routes: /install, /version, /exit.
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "voxd.h"
#include "http_server.h"
#include "file_registry.h"
#include "inflate.h"
#include "app_installer.h"

atomic_int http_keep_running = 1;
atomic_int install_completed = 0;

#define MAX_REQUEST 8192
#define CORS_ORIGIN "*"

static int listen_fd = -1;

static const FileEntry *registry_lookup(const char *url) {
    if (strcmp(url, ROUTE_INDEX) == 0)
        return file_registry_find(ROUTE_INDEX_HTML);
    return file_registry_find(url);
}

/* Returns a pointer to the first occurrence of \r\n\r\n in buf[0..len), or NULL. */
static const char *element_headers_end(const char *buf, size_t len) {
    if (len < 4)
        return NULL;
    for (size_t i = 0; i + 4 <= len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' &&
            buf[i + 2] == '\r' && buf[i + 3] == '\n')
            return &buf[i];
    }
    return NULL;
}

/* Serve one HTTP request on a connected socket. Returns bytes sent on the
 * body, or the negative errno-ish code. */
static int handle_connection(int fd, const char *req) {
    /* request line: GET /path?query HTTP/1.1 */
    char method[16];
    char raw_path[MAX_REQUEST];
    int parsed = sscanf(req, "%15s %2000s", method, raw_path);
    if (parsed != 2) {
        const char *bad = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        return (int)send(fd, bad, strlen(bad), 0) < 0 ? -1 : 0;
    }

    /* Split off the query string for file lookup; AppCache requests carry
     * ?v=final etc. which we must strip before registry lookup. */
    char path[MAX_REQUEST];
    size_t qpos = strcspn(raw_path, "?");
    if (qpos >= sizeof(path))
        qpos = sizeof(path) - 1;
    memcpy(path, raw_path, qpos);
    path[qpos] = '\0';

    const char *body = NULL;
    size_t body_size = 0;
    const char *ctype = "text/plain; charset=utf-8";
    int status = 200;
    unsigned char *decompressed = NULL;

    if (strcmp(path, ROUTE_INSTALL) == 0) {
        int err = voxd_install_app_if_needed();
        if (err == 0) {
            body = "OK";
            body_size = 2;
            atomic_store(&install_completed, 1);
            atomic_store(&http_keep_running, 0);
        } else {
            body = "Install failed";
            body_size = 14;
            status = 500;
        }
    } else if (strcmp(path, ROUTE_EXIT) == 0) {
        body = "OK";
        body_size = 2;
        atomic_store(&http_keep_running, 0);
    } else if (strcmp(path, ROUTE_VERSION) == 0) {
        body = VOXD_FULL_VERSION;
        body_size = strlen(VOXD_FULL_VERSION);
    } else {
        const FileEntry *entry = registry_lookup(path);
        if (!entry) {
            body = "404 Not Found";
            body_size = 13;
            status = 404;
        } else {
            if (entry->compressed) {
                decompressed = malloc(entry->orig_size);
                if (!decompressed) {
                    body = "503 Out of Memory";
                    body_size = 17;
                    status = 503;
                } else {
                    unsigned long destlen = entry->orig_size;
                    unsigned long sourcelen = entry->size;
                    if (puff(decompressed, &destlen, entry->data, &sourcelen) != 0) {
                        free(decompressed);
                        decompressed = NULL;
                        body = "500 Inflate Error";
                        body_size = 17;
                        status = 500;
                    } else {
                        body = (const char *)decompressed;
                        body_size = destlen;
                    }
                }
            } else {
                body = (const char *)entry->data;
                body_size = entry->size;
            }
            ctype = entry->content_type ? entry->content_type : "application/octet-stream";
        }
        (void)ctype;
    }

    /* Build response. Keep it simple: HTTP/1.1 with Content-Length + close. */
    char head[1024];
    int hl = snprintf(head, sizeof(head),
                      "HTTP/1.1 %d %s\r\n"
                      "Content-Type: %s\r\n"
                      "Content-Length: %zu\r\n"
                      "Cache-Control: no-cache, must-revalidate\r\n"
                      "Access-Control-Allow-Origin: %s\r\n"
                      "Connection: close\r\n"
                      "\r\n",
                      status, status == 200 ? "OK" : (status == 404 ? "Not Found" : "Internal Server Error"),
                      ctype, body_size, CORS_ORIGIN);

    if (hl < 0 || hl >= (int)sizeof(head))
        hl = (int)sizeof(head) - 1;

    if (send(fd, head, (size_t)hl, 0) < 0)
        goto out;
    if (body_size > 0 && send(fd, body, body_size, 0) < 0)
        goto out;

out:
    if (decompressed)
        free(decompressed);
    return 0;
}

static void *connection_thread(void *arg) {
    int fd = (int)(intptr_t)arg;
    char req[MAX_REQUEST];
    size_t used = 0;

    /* Read until end-of-headers (\r\n\r\n) or the buffer fills. HTTP request
     * headers can arrive over multiple TCP segments, so a single recv() is
     * not enough. */
    while (used < sizeof(req) - 1) {
        ssize_t got = recv(fd, req + used, sizeof(req) - 1 - used, 0);
        if (got <= 0)
            break;
        used += (size_t)got;
        if (used >= 4 &&
            element_headers_end(req, used) != NULL)
            break;
    }
    req[used] = '\0';

    if (used > 0)
        handle_connection(fd, req);
    close(fd);
    return NULL;
}

int http_server_run(void) {
    struct sockaddr_in sa;
    int one = 1;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        voxd_log("socket failed\n");
        return -1;
    }
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)VOXD_PORT);
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(listen_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        voxd_log("bind failed on port %d\n", VOXD_PORT);
        close(listen_fd);
        listen_fd = -1;
        return -1;
    }
    if (listen(listen_fd, 16) < 0) {
        voxd_log("listen failed\n");
        close(listen_fd);
        listen_fd = -1;
        return -1;
    }

    voxd_log("server listening on 127.0.0.1:%d\n", VOXD_PORT);

    while (atomic_load(&http_keep_running)) {
        struct sockaddr_in peer;
        socklen_t plen = sizeof(peer);
        int cf = accept(listen_fd, (struct sockaddr *)&peer, &plen);
        if (cf < 0)
            break; /* listener closed by http_server_stop() */

        pthread_t tid;
        if (pthread_create(&tid, NULL, connection_thread, (void *)(intptr_t)cf) == 0) {
            pthread_detach(tid);
        } else {
            close(cf);
        }
    }

    if (listen_fd >= 0) {
        close(listen_fd);
        listen_fd = -1;
    }
    return 0;
}

void http_server_stop(void) {
    atomic_store(&http_keep_running, 0);
    if (listen_fd >= 0) {
        /* closing the FD makes the blocking accept() return */
        close(listen_fd);
        listen_fd = -1;
    }
}