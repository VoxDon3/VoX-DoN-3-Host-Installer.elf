#pragma once

#include <stdio.h>

/* stdout on a payload is usually the logger console; make it cheap to swap. */
#define voxd_log(fmt, ...) printf("[VOXD] " fmt, ##__VA_ARGS__)