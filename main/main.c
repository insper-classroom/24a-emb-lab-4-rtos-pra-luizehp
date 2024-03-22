#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

const int trig = 13;
const int echo = 12;

SemaphoreHandle_t xSemaphoreTrigger;
QueueHandle_t xQueueTime,xQueueDistance;

void gpio_callback(uint gpio, uint32_t events) {
    static uint32_t rise, fall, dt;
    if(events == 0x8) {  // rise edge
        if (gpio == echo) {
            rise = to_us_since_boot(get_absolute_time());
        }
    } else if (events == 0x4) { // fall edge
        if (gpio == echo) {
            fall = to_us_since_boot(get_absolute_time());
            dt = fall-rise;
            xQueueSendFromISR(xQueueTime, &dt, 0);
        }
    } 
}


void echo_task(void *p){
    gpio_init(echo);
    gpio_set_dir(echo, GPIO_IN);
    gpio_set_irq_enabled_with_callback(echo, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
    
    uint32_t dt;
    while (true) {
        if (xQueueReceive(xQueueTime, &dt,  0)) {
            double distance = (343000 * (dt) / 10000000.0) / 2.0; 
            xQueueSend(xQueueDistance, &distance, 0);
        }
    }
}

void trigger_task(void *p){
    gpio_init(trig);
    gpio_set_dir(trig, GPIO_OUT);

    while (true){
        gpio_put(trig, 1);
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_put(trig, 0);
        xSemaphoreGive(xSemaphoreTrigger);
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}


void oled_task(void *p){
    
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);
   

    double distance;
    char distance_str[20]; 
    while(true){
        if (xSemaphoreTake(xSemaphoreTrigger, 0) == pdTRUE) {
            if (xQueueReceive(xQueueDistance, &distance,  0)) {
                printf("Distancia: %f\n\n", distance);
                sprintf(distance_str, "%d cm", (int)distance); 

                gfx_clear_buffer(&disp);
                gfx_draw_string(&disp, 0, 0, 1, "Distancia: ");
                gfx_draw_string(&disp, 0, 10, 1, distance_str);

                gfx_draw_line(&disp, 0, 20, (int)distance, 20);
                gfx_show(&disp);
            } else {
                printf("Distancia: [FALHA]\n\n");

                gfx_clear_buffer(&disp);
                gfx_draw_string(&disp, 0, 0, 1, "Distancia: ");
                gfx_draw_string(&disp, 0, 10, 1, "[FALHA]");
                gfx_show(&disp);
            }
       } 
    }
}

int main() {
    stdio_init_all();

    xSemaphoreTrigger = xSemaphoreCreateBinary();
    xQueueTime = xQueueCreate(5, sizeof(uint32_t));
    xQueueDistance = xQueueCreate(5, sizeof(double));

    xTaskCreate(trigger_task, "Trigger task", 4095, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo task", 4095, NULL, 1, NULL);
    xTaskCreate(oled_task, "Oled task", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
