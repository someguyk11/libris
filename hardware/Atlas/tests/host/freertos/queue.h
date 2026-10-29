#pragma once
#include "FreeRTOS.h"
#include <cstddef>
using QueueHandle_t=void*;
QueueHandle_t xQueueCreate(unsigned capacity,size_t bytes);
void vQueueDelete(QueueHandle_t queue);
BaseType_t xQueueSend(QueueHandle_t queue,const void* data,TickType_t wait);
BaseType_t xQueueReceive(QueueHandle_t queue,void* data,TickType_t wait);
BaseType_t xQueueOverwrite(QueueHandle_t queue,const void* data);
