#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "pico/bootrom.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include <stdio.h>

#define led_pin_green 11
#define led_pin_blue 12
#define led_pin_red 13
#define botao_A 5
#define botaoB 6
#define BUZZER 10
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

// Variáveis globais
volatile bool modo = true;
volatile bool cor = true;  // Variável para utilizar funções no display
ssd1306_t ssd;             // Inicializa a estrutura do display

void vBlinkTask(){

    while (true){
        gpio_put(led_pin_green, true);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_put(led_pin_red, true);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_put(led_pin_blue, false);
        gpio_put(led_pin_green, false);
        gpio_put(led_pin_red, true);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_put(led_pin_red, false);
        printf("Blink\n");
    }
}

// Função para desenho no display
void vDisplayTask(){

    while(true){
        ssd1306_fill(&ssd, !cor);                                   // Limpa o display
        ssd1306_rect(&ssd, 3, 90, 21, 18, cor, cor);                // Retângulo cheio
        ssd1306_rect(&ssd, 22, 90, 21, 18, cor, !cor);              // Retângulo vazio
        ssd1306_rect(&ssd, 41, 90, 21, 18, cor, !cor);              // Retângulo vazio
        ssd1306_rect(&ssd, 3, 90, 3, 56, cor, cor);                // Retângulo cheio esquerdo
        ssd1306_rect(&ssd, 3, 108, 3, 56, cor, cor);                // Retângulo cheio direito
        ssd1306_send_data(&ssd);                                    // Atualiza o display
        vTaskDelay(pdMS_TO_TICKS(980));
        ssd1306_fill(&ssd, !cor);                                   // Limpa o display
        ssd1306_rect(&ssd, 3, 90, 21, 18, cor, !cor);                // Retângulo vazio
        ssd1306_rect(&ssd, 22, 90, 21, 18, cor, cor);              // Retângulo cheio
        ssd1306_rect(&ssd, 41, 90, 21, 18, cor, !cor);              // Retângulo vazio
        ssd1306_rect(&ssd, 3, 90, 3, 56, cor, cor);                // Retângulo cheio esquerdo
        ssd1306_rect(&ssd, 3, 108, 3, 56, cor, cor);                // Retângulo cheio direito
        ssd1306_send_data(&ssd);                                    // Atualiza o display
        vTaskDelay(pdMS_TO_TICKS(980));
        ssd1306_fill(&ssd, !cor);                                   // Limpa o display
        ssd1306_rect(&ssd, 3, 90, 21, 18, cor, !cor);                // Retângulo vazio
        ssd1306_rect(&ssd, 22, 90, 21, 18, cor, !cor);              // Retângulo vazio
        ssd1306_rect(&ssd, 41, 90, 21, 18, cor, cor);              // Retângulo cheio
        ssd1306_rect(&ssd, 3, 90, 3, 56, cor, cor);                // Retângulo cheio esquerdo
        ssd1306_rect(&ssd, 3, 108, 3, 56, cor, cor);                // Retângulo cheio direito
        ssd1306_send_data(&ssd);                                    // Atualiza o display
        vTaskDelay(pdMS_TO_TICKS(980));
    }
}

// Função para soar o alarme
void sirene(uint freq_grave, uint freq_agudo, uint duration) {
    uint slice_num = pwm_gpio_to_slice_num(BUZZER);
    pwm_set_enabled(slice_num, true);
    uint clock = 125000000; // Frequência do clock do PWM (125 MHz)
    uint wrap_value_grave = clock / freq_grave; // Valor de wrap para a frequência grave
    uint wrap_value_agudo = clock / freq_agudo; // Valor de wrap para a frequência "aguda"

    absolute_time_t start_time = get_absolute_time();
    while (absolute_time_diff_us(start_time, get_absolute_time()) < duration * 1000) {
        // Toca a frequência grave (PEN)
        pwm_set_wrap(slice_num, wrap_value_grave);
        pwm_set_gpio_level(BUZZER, wrap_value_grave/3); // Ciclo de trabalho de 50%
        sleep_ms(200); // Tempo de cada "PEN"

        // Toca a frequência "aguda" (PEN)
        pwm_set_wrap(slice_num, wrap_value_agudo);
        pwm_set_gpio_level(BUZZER, wrap_value_agudo/3); // Ciclo de trabalho de 50%
        sleep_ms(200); // Tempo de cada "PEN"
    }

    // Desliga o BUZZER
    pwm_set_gpio_level(BUZZER, 0);
}

void vBuzzerTask(){
    while(true){
        sirene(200, 200, 500);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void gpio_irq_handler(uint gpio, uint32_t events){
    if(gpio == botaoB)
        reset_usb_boot(0, 0);
    
    if(gpio == botao_A)
        modo = !modo;
}


void setup(){
    // Inicialização do LED RGB
    gpio_init(led_pin_red);
    gpio_set_dir(led_pin_red, GPIO_OUT);

    gpio_init(led_pin_blue);
    gpio_set_dir(led_pin_blue, GPIO_OUT);

    gpio_init(led_pin_green);
    gpio_set_dir(led_pin_green, GPIO_OUT);

    // Para ser utilizado o modo BOOTSEL com botão B
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL,
                                         true, &gpio_irq_handler);
    // Fim do trecho para modo BOOTSEL com botão B

    // Inicialização do botão A
    gpio_init(botao_A);
    gpio_set_dir(botao_A, GPIO_IN);
    gpio_pull_up(botao_A);
    gpio_set_irq_enabled_with_callback(botao_A, GPIO_IRQ_EDGE_FALL,
        true, &gpio_irq_handler);

     // Inicializa o PWM do buzzer
     gpio_init(BUZZER);
     gpio_set_function(BUZZER, GPIO_FUNC_PWM);

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL);                                        // Pull up the clock line
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd);                                         // Configura o display
    ssd1306_send_data(&ssd);                                      // Envia os dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
 
}

int main(){
   
    setup();
    stdio_init_all();

    xTaskCreate(vBlinkTask, "Blink Task", 
        configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    /* xTaskCreate(vBuzzerTask, "Buzzer Task",
         configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);  */
    xTaskCreate(vDisplayTask, "Display Task",
         configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL); 
    vTaskStartScheduler();
    panic_unsupported();
}
