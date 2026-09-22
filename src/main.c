#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <unistd.h>

#include "voxd.h"
#include "http_server.h"
#include "ps5_launcher.h"

static pid_t find_pid(const char *name) {
  int mib[4] = {1, 14, 8, 0};
  pid_t mypid = getpid();
  pid_t pid = -1;
  size_t buf_size;
  uint8_t *buf;

  if (sysctl(mib, 4, 0, &buf_size, 0, 0)) {
    voxd_log("sysctl failed\n");
    return -1;
  }

  if (!(buf = malloc(buf_size))) {
    voxd_log("malloc failed\n");
    return -1;
  }

  if (sysctl(mib, 4, buf, &buf_size, 0, 0)) {
    voxd_log("sysctl failed\n");
    free(buf);
    return -1;
  }

  /* KERN_PROC_ALL scan — raw offsets into struct kinfo_proc as exposed by the
   * PS5 kernel: ki_pid at offset 72, ki_tdname at 447. */
  for (uint8_t *ptr = buf; ptr < (buf + buf_size);) {
    int ki_structsize = *(int *)ptr;
    pid_t ki_pid = *(pid_t *)&ptr[72];
    char *ki_tdname = (char *)&ptr[447];

    ptr += ki_structsize;
    if (!strcmp(name, ki_tdname) && ki_pid != mypid) {
      pid = ki_pid;
    }
  }

  free(buf);
  return pid;
}

extern int sceNetCtlInit();
extern int sceUserServiceInitialize(void *);

__attribute__((used)) volatile const char voxd_version_sig[] =
    "VOXD_VER:" VOXD_FULL_VERSION;

int main(void) {
  pthread_t server_tid;
  pid_t pid;

  syscall(SYS_thr_set_name, -1, VOXD_THREAD_NAME);

  /* Kill previous installer instances */
  while ((pid = find_pid(VOXD_THREAD_NAME)) > 0) {
    if (kill(pid, SIGKILL)) {
      voxd_log("kill failed\n");
      return EXIT_FAILURE;
    }
    sleep(1);
  }

  voxd_log("VoX DoN 3 Host Installer v%s starting on port %d...\n",
           VOXD_FULL_VERSION, VOXD_PORT);

  int err;
  if ((err = sceNetCtlInit()) == 0) {
    voxd_log("Network Controller initialized.\n");
  } else {
    voxd_log("sceNetCtlInit failed: 0x%08X\n", err);
  }

  int user_prio = 256;
  if ((err = sceUserServiceInitialize(&user_prio)) == 0) {
    voxd_log("User Service initialized.\n");
  } else {
    voxd_log("sceUserServiceInitialize failed: 0x%08X\n", err);
  }

  signal(SIGPIPE, SIG_IGN);
  signal(SIGHUP, SIG_IGN);
  signal(SIGTERM, SIG_IGN);

  if (pthread_create(&server_tid, NULL, (void *(*)(void *))http_server_run,
                     NULL) != 0) {
    voxd_log("Failed to start HTTP server thread\n");
    return 1;
  }

  char browser_url[256];
  snprintf(browser_url, sizeof(browser_url),
           "http://127.0.0.1:%d/?v=%s",
           VOXD_PORT, VOXD_FULL_VERSION);

  voxd_log("Launching browser...\n");
  voxd_launch_browser(browser_url);

  /* Main loop — runs until /install succeeds (sets http_keep_running = 0) */
  while (atomic_load(&http_keep_running)) {
    usleep(100000);
  }

  if (atomic_load(&install_completed)) {
    voxd_notify("VoX DoN 3 Host cached successfully!");
  }

  http_server_stop();
  pthread_join(server_tid, NULL);

  sleep(1);
  return 0;
}