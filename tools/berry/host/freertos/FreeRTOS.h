#pragma once

// The FreeRTOS names the machine's headers use, for the host. The only one the compiled sources
// need is the command queue: the panel's native modules post to it and the check drains it.

#include <stddef.h>
#include <stdint.h>

typedef long BaseType_t;
typedef unsigned long UBaseType_t;
typedef uint32_t TickType_t;
typedef void* TaskHandle_t;

#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))

struct HostQueue;
typedef HostQueue* QueueHandle_t;
