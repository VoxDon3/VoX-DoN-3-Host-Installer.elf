#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "voxd.h"
#include "notification.h"
#include <stdbool.h>

#define SCE_NOTIFICATION_LOCAL_USER_ID_SYSTEM 0xFE

int sceNotificationSend(int userId, bool isLogged, const char *payload);

static const char TEMPLATE[] =
  "{\n"
  "  \"rawData\": {\n"
  "    \"viewTemplateType\": \"EEED\",\n"
  "    \"useCaseId\": \"IDC\",\n"
  "    \"priority\": 80,\n"
  "    \"viewData\": {\n"
  "      \"message\": {\n"
  "        \"body\": \"%s\"\n"
  "      },\n"
  "      \"subMessage\": {\n"
  "        \"body\": \"%s\"\n"
  "      }\n"
  "    }\n"
  "  }\n"
  "}";

/* Format a message and send it as a PS5 notification toast. */
void voxd_notify(const char *fmt, ...) {
  char msg[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

  char payload[768];
  snprintf(payload, sizeof(payload), TEMPLATE, msg, "VoX DoN 3 Host");
  sceNotificationSend(SCE_NOTIFICATION_LOCAL_USER_ID_SYSTEM, true, payload);

  voxd_log("notify: %s\n", msg);
}