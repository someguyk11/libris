#pragma once
#include "FreeRTOS.h"
using TaskHandle_t=void*;
BaseType_t xTaskCreatePinnedToCore(void(*entry)(void*),const char* name,unsigned stack,void* context,unsigned priority,TaskHandle_t* task,int core);
void vTaskDelay(TickType_t ticks);
