#include <stdio.h>

#include "voxd.h"
#include "ps5_launcher.h"

int sceSystemServiceLaunchWebBrowser(const char *uri, void *arg);

int voxd_launch_browser(const char *uri) {
  voxd_log("Launching browser: %s\n", uri);
  if (sceSystemServiceLaunchWebBrowser(uri, 0) != 0) {
    voxd_notify("VoX DoN 3: Failed to launch browser");
    return -1;
  }
  return 0;
}