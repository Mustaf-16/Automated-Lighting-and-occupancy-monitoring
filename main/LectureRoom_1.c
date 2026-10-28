#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#define ROOM_ID 1

#define PIN_PIR              27
#define PIN_RELAY            26     
#define PIN_OCC_LED          19     
#define RELAY_ON_LEVEL       1      

#define VACANCY_TIMEOUT_S    300     
#define CONTROL_TICK_MS      250     
#define STATUS_PERIOD_S      20     

static const char *TAG = "lecture_room";

typedef struct {
    bool     occupied;
    bool     light_on;
    int64_t  last_motion_us;
    uint32_t motion_events;     /* times the room went vacant -> occupied */
} room_state_t;

static room_state_t      s_room;
static SemaphoreHandle_t s_room_mutex;
static TaskHandle_t      s_pir_task_handle = NULL; /* Handle to notify pir_task from ISR */

/* Call with s_room_mutex held: make outputs match the state. */
static void apply_outputs_locked(void)
{
    bool want_light = s_room.occupied;
    if (want_light != s_room.light_on) {
        s_room.light_on = want_light;
        gpio_set_level(PIN_RELAY, want_light ? RELAY_ON_LEVEL : !RELAY_ON_LEVEL);
        ESP_LOGI(TAG, "Light -> %s", want_light ? "ON" : "OFF");
    }
    gpio_set_level(PIN_OCC_LED, s_room.occupied);
}

/* Interrupt Service Routine (ISR) triggered by PIR motion pin */
static void IRAM_ATTR pir_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (s_pir_task_handle != NULL) {
        /* Unblock pir_task directly from the ISR */
        vTaskNotifyGiveFromISR(s_pir_task_handle, &xHigherPriorityTaskWoken);
        
        /* Force a context switch if pir_task has a higher priority than the current task */
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

static void gpio_setup(void)
{
    /* Configure PIR pin for rising edge interrupt (triggers when PIR goes HIGH) */
    gpio_config_t in = {
        .pin_bit_mask = 1ULL << PIN_PIR,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    gpio_config(&in);

    gpio_config_t out = {
        .pin_bit_mask = (1ULL << PIN_RELAY) | (1ULL << PIN_OCC_LED),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&out);

    gpio_set_level(PIN_RELAY, !RELAY_ON_LEVEL);     /* light OFF at boot */
    gpio_set_level(PIN_OCC_LED, 0);

    /* Install global GPIO ISR service and register handler for the PIR pin */
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_PIR, pir_isr_handler, NULL);
}

/* Interrupt-driven PIR Task */
static void pir_task(void *arg)
{
    while(1) {
        /* Wait indefinitely (portMAX_DELAY) until notified by pir_isr_handler */
        uint32_t ulNotificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (ulNotificationValue > 0) {
            bool newly_occupied = false;

            xSemaphoreTake(s_room_mutex, portMAX_DELAY);
            s_room.last_motion_us = esp_timer_get_time();
            if (!s_room.occupied) {
                s_room.occupied = true;
                s_room.motion_events++;
                newly_occupied = true;
            }
            apply_outputs_locked();
            xSemaphoreGive(s_room_mutex);

            if (newly_occupied) {
                ESP_LOGI(TAG, "Motion detected (ISR): room OCCUPIED");
            }
        }
    }
}

static void control_task(void *arg)
{
    const int64_t timeout_us = (int64_t)VACANCY_TIMEOUT_S * 1000000LL;
    int status_ticks = 0;

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(CONTROL_TICK_MS));

        bool vacated = false;
        room_state_t snap;
        int64_t idle_us;

        xSemaphoreTake(s_room_mutex, portMAX_DELAY);
        idle_us = esp_timer_get_time() - s_room.last_motion_us;
        if (s_room.occupied && idle_us > timeout_us) {
            s_room.occupied = false;
            vacated = true;
        }
        apply_outputs_locked();
        snap = s_room;                      /* consistent copy for printing */
        xSemaphoreGive(s_room_mutex);

        if (vacated) {
            ESP_LOGI(TAG, "No motion for %d s: room VACANT", VACANCY_TIMEOUT_S);
        }

        if (++status_ticks >= (STATUS_PERIOD_S * 1000 / CONTROL_TICK_MS)) {
            status_ticks = 0;
            if (snap.occupied) {
                int left = VACANCY_TIMEOUT_S - (int)(idle_us / 1000000LL);
                if (left < 0) left = 0;
                ESP_LOGI(TAG, "STATUS: occupied | light %s | vacant in %d s | events %lu",
                         snap.light_on ? "ON" : "OFF", left,
                         (unsigned long)snap.motion_events);
            } else {
                ESP_LOGI(TAG, "STATUS: vacant   | light %s | events %lu",
                         snap.light_on ? "ON" : "OFF",
                         (unsigned long)snap.motion_events);
            }
        }
    }
}

void app_main(void)
{
    s_room_mutex = xSemaphoreCreateMutex();

    /* 1. Create the PIR task first and store its handle */
    xTaskCreate(pir_task, "pir", 2048, NULL, 5, &s_pir_task_handle);
    
    /* 2. Create the control task */
    xTaskCreate(control_task, "control", 3072, NULL, 4, NULL);

    /* 3. Setup GPIO and attach ISR after task handle is initialized */
    gpio_setup();

    ESP_LOGI(TAG, "Lecture room controller started (Interrupt-driven, timeout %d s)", VACANCY_TIMEOUT_S);
}