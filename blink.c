#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

#define led_pin_red 12

void vBlinkTask()
{
    gpio_init(led_pin_red);
    gpio_set_dir(led_pin_red, GPIO_OUT);
    while (true)
    {
        gpio_put(led_pin_red, true);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_put(led_pin_red, false);
        vTaskDelay(pdMS_TO_TICKS(200));
        printf("Blink\n");
    }
}

// Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}

int main()
{
    // Para ser utilizado o modo BOOTSEL com botão B
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL,
                                         true, &gpio_irq_handler);
    // Fim do trecho para modo BOOTSEL com botão B

    stdio_init_all();

    xTaskCreate(vBlinkTask, "Blink Task", 
        configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    vTaskStartScheduler();
    panic_unsupported();
}
