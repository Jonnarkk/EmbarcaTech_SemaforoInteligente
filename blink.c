#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "pico/bootrom.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "lib/led_matriz.h"
#include "lib/buzzer.h"
#include "hardware/pio.h"
#include "pio_matriz.pio.h"
#include "hardware/clocks.h"
#include <stdio.h>

#define PINOA 16
#define PINOB 17
#define PINOC 18
#define PINOD 19
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

// --- Variáveis Globais de Estado ---

// Enum para definir os estados do semáforo de forma clara
typedef enum {
    ESTADO_VERDE,
    ESTADO_AMARELO,
    ESTADO_VERMELHO,
    ESTADO_NOTURNO_PISCANTE,
    ESTADO_NOTURNO_APAGADO
} SemaforoState_t;

volatile bool modo_noturno = false;
volatile SemaforoState_t g_estadoSemaforo = ESTADO_VERMELHO; // Inicia no estado vermelho
volatile int g_tempoRestante = 0; // Para o countdown do display de 7 segmentos
volatile bool botao_pressionado = false; // Flag para a interrupção do botão

ssd1306_t ssd;
volatile bool cor = true; // Para usar em ssd1306_rect


// --- Handles das Tarefas ---
TaskHandle_t xTaskControleModo = NULL;
TaskHandle_t xTaskSemaforoNormal = NULL;
TaskHandle_t xTaskSemaforoNoturno = NULL;
TaskHandle_t xTaskGerenciaHardware = NULL;
TaskHandle_t xTaskDisplay7Seg = NULL;
TaskHandle_t xTaskBuzzer = NULL;

// Função para enviar valor BCD para o CI 7447
void escrever_display_7_seg(uint8_t numero) {
    if (numero > 9) numero = 9;
    gpio_put(PINOA, (numero >> 0) & 1); // Bit 0 (LSB)
    gpio_put(PINOB, (numero >> 1) & 1); // Bit 1
    gpio_put(PINOC, (numero >> 2) & 1); // Bit 2
    gpio_put(PINOD, (numero >> 3) & 1); // Bit 3 (MSB)
}

// Tarefa ESCRAVA para o Display de 7 Segmentos
void vTaskDisplay7Seg(void *pvParameters) {
    while (true) {
        if (!modo_noturno) {
            escrever_display_7_seg(g_tempoRestante);
        } else {
            escrever_display_7_seg(0); // Mostra 0 ou apaga no modo noturno
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Taxa de atualização rápida
    }
}

// Tarefa ESCRAVA que gerencia todo o hardware visual (LEDs, Matriz, Display OLED)
void vTaskGerenciaHardware(void *pvParameters) {
    // Inicialização do PIO para a matriz (feita uma vez no início da tarefa)
    PIO pio = pio0;
    uint sm = pio_init(pio);

    while (true) {
        SemaforoState_t estado_atual = g_estadoSemaforo;

        // 1. Atualiza LED RGB
        if (estado_atual == ESTADO_NOTURNO_PISCANTE || estado_atual == ESTADO_AMARELO) {
            gpio_put(led_pin_red, true);
            gpio_put(led_pin_green, true);
            gpio_put(led_pin_blue, false);
        } else {
            gpio_put(led_pin_red,   estado_atual == ESTADO_VERMELHO);
            gpio_put(led_pin_green, estado_atual == ESTADO_VERDE);
            gpio_put(led_pin_blue,  false);
        }

        // 2. Prepara e envia o desenho da Matriz de LEDs
        switch (estado_atual) {
            case ESTADO_VERDE:   desenhar_seta_direita(); break;
            case ESTADO_AMARELO:
            case ESTADO_NOTURNO_PISCANTE: desenhar_exclamacao(); break;
            case ESTADO_VERMELHO: desenhar_proibido(); break;
            case ESTADO_NOTURNO_APAGADO: limpar_todos_leds(); break;
        }
        desenho_pio(0, pio, sm); 
        
        // ALTERAÇÃO: A lógica abaixo foi restaurada para usar a sua arte original do semáforo em pé.
        // --- 3. Atualiza Display OLED com a SUA ARTE ---
        ssd1306_fill(&ssd, !cor); // Limpa o display
        
        // Desenha os textos
        ssd1306_draw_string(&ssd, "VERDE", 10, 10);
        ssd1306_draw_string(&ssd, "AMARELO", 10, 30);
        ssd1306_draw_string(&ssd, "VERMELHO", 10, 50);

        switch (estado_atual) {
            case ESTADO_VERDE:
                ssd1306_rect(&ssd, 90, 10, 21, 18, cor, cor);  // Retângulo cheio (Verde)
                ssd1306_rect(&ssd, 90, 30, 21, 18, cor, !cor); // Retângulo vazio
                ssd1306_rect(&ssd, 90, 50, 21, 18, cor, !cor); // Retângulo vazio
                break;

            case ESTADO_AMARELO:
                ssd1306_rect(&ssd, 90, 10, 21, 18, cor, !cor);
                ssd1306_rect(&ssd, 90, 30, 21, 18, cor, cor);  // Retângulo cheio (Amarelo)
                ssd1306_rect(&ssd, 90, 50, 21, 18, cor, !cor);
                break;

            case ESTADO_VERMELHO:
                ssd1306_rect(&ssd, 90, 10, 21, 18, cor, !cor);
                ssd1306_rect(&ssd, 90, 30, 21, 18, cor, !cor);
                ssd1306_rect(&ssd, 90, 50, 21, 18, cor, cor);  // Retângulo cheio (Vermelho)
                break;

            case ESTADO_NOTURNO_PISCANTE:
                ssd1306_rect(&ssd, 90, 30, 21, 18, cor, cor); // Amarelo aceso
                break;

            case ESTADO_NOTURNO_APAGADO:
                // Todos vazios
                ssd1306_rect(&ssd, 90, 10, 21, 18, cor, !cor);
                ssd1306_rect(&ssd, 90, 30, 21, 18, cor, !cor);
                ssd1306_rect(&ssd, 90, 50, 18, 18, cor, !cor);
                break;
        }
        ssd1306_send_data(&ssd); // Envia o buffer final para o display

        vTaskDelay(pdMS_TO_TICKS(100)); // Taxa de atualização desta tarefa
    }
}


// Tarefa MESTRE para o modo normal - Controla o tempo e o estado
void vSemaforoControleNormal(void *pvParameters) {
    const int tempo_verde_s = 6;
    const int tempo_amarelo_s = 3;
    const int tempo_vermelho_s = 9;
    const int passo_delay_ms = 100;

    while (true) {
        // Se a tarefa foi resumida no modo errado, ela se auto-suspende novamente
        if (modo_noturno) {
            vTaskSuspend(NULL);
        }

        g_estadoSemaforo = ESTADO_VERDE;
        for (int i = tempo_verde_s; i > 0 && !modo_noturno; i--) {
            g_tempoRestante = i;
            for (int j = 0; j < 1000 / passo_delay_ms; j++) {
                if (modo_noturno) break; // Sai do loop de 1s
                vTaskDelay(pdMS_TO_TICKS(passo_delay_ms));
            }
        }
        if (modo_noturno) continue; // Volta ao topo do while(true) para suspender

        g_estadoSemaforo = ESTADO_AMARELO;
        for (int i = tempo_amarelo_s; i > 0 && !modo_noturno; i--) {
            g_tempoRestante = i;
            for (int j = 0; j < 1000 / passo_delay_ms; j++) {
                if (modo_noturno) break;
                vTaskDelay(pdMS_TO_TICKS(passo_delay_ms));
            }
        }
        if (modo_noturno) continue;

        g_estadoSemaforo = ESTADO_VERMELHO;
        for (int i = tempo_vermelho_s; i > 0 && !modo_noturno; i--) {
            g_tempoRestante = i;
            for (int j = 0; j < 1000 / passo_delay_ms; j++) {
                if (modo_noturno) break;
                vTaskDelay(pdMS_TO_TICKS(passo_delay_ms));
            }
        }
    }
}

// Tarefa MESTRE para o modo noturno
void vSemaforoControleNoturno(void *pvParameters) {
    while (true) {
        g_estadoSemaforo = ESTADO_NOTURNO_PISCANTE;
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (!modo_noturno) vTaskSuspend(NULL);

        g_estadoSemaforo = ESTADO_NOTURNO_APAGADO;
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (!modo_noturno) vTaskSuspend(NULL);
    }
}

// Tarefa que gerencia a troca de modos
void vTaskControleModo(void *pvParameters) {
    absolute_time_t last_press = 0;
    while (true) {
        if (botao_pressionado) {
            if (absolute_time_diff_us(last_press, get_absolute_time()) > 200000) {
                last_press = get_absolute_time();
                modo_noturno = !modo_noturno;
                if (modo_noturno) {
                    vTaskSuspend(xTaskSemaforoNormal);
                    vTaskResume(xTaskSemaforoNoturno);
                } else {
                    vTaskSuspend(xTaskSemaforoNoturno);
                    vTaskResume(xTaskSemaforoNormal);
                }
            }
            botao_pressionado = false;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Tarefa do Buzzer sincronizada com o estado global
void vBuzzerTask(void *pvParameters) {
    const int passo_delay_ms = 100;

    while (true) {
        SemaforoState_t estado_atual = g_estadoSemaforo;

        // A lógica do som só é executada se o estado for um dos que produzem som
        switch (estado_atual) {
            case ESTADO_VERDE:
                buzz(BUZZER, 800, 800);
                vTaskDelay(pdMS_TO_TICKS(200)); // Pausa curta para completar 1s de ciclo
                break;
            case ESTADO_AMARELO:
            case ESTADO_NOTURNO_PISCANTE: // Mesmo som para amarelo e noturno piscante
                for (int i = 0; i < 5; i++) {
                    if (g_estadoSemaforo != estado_atual) break; // Interrompe se o estado mudou
                    buzz(BUZZER, 400, 100);
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
                vTaskDelay(pdMS_TO_TICKS(100)); // Pausa para não sobrecarregar
                break;
            case ESTADO_VERMELHO:
                buzz(BUZZER, 700, 500);
                for (int i = 0; i < 10; i++) { // Espera 1s de forma interruptível
                    if (g_estadoSemaforo != ESTADO_VERMELHO) break;
                    vTaskDelay(pdMS_TO_TICKS(passo_delay_ms));
                }
                break;
            default:
                // Para ESTADO_NOTURNO_APAGADO ou outros, não faz som e apenas espera.
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
        }
    }
}


// Função Handler da interrupção do botão
void gpio_irq_handler(uint gpio, uint32_t events) {
    if (gpio == botao_A) {
        botao_pressionado = true;
    }
    if (gpio == botaoB) {
        PIO pio = pio0;
        uint sm = 0;
        uint offset = pio_add_program(pio, &pio_matriz_program);
        pio_matriz_program_init(pio, sm, offset, pino_matriz);

        limpar_todos_leds();
        desenho_pio(0, pio, sm);

        ssd1306_fill(&ssd, !cor);                                   // Limpa o display
        ssd1306_send_data(&ssd);                                    // Atualiza o display

        reset_usb_boot(0, 0);
    }
}




void setup(){
    // Inicialização do LED RGB
    gpio_init(led_pin_red);
    gpio_set_dir(led_pin_red, GPIO_OUT);

    gpio_init(led_pin_blue);
    gpio_set_dir(led_pin_blue, GPIO_OUT);

    gpio_init(led_pin_green);
    gpio_set_dir(led_pin_green, GPIO_OUT);

    // Inicialização dos Pinos GPIO
    gpio_init(PINOA);
    gpio_set_dir(PINOA, GPIO_OUT);
    
    gpio_init(PINOB);
    gpio_set_dir(PINOB, GPIO_OUT);

    gpio_init(PINOC);
    gpio_set_dir(PINOC, GPIO_OUT);

    gpio_init(PINOC);
    gpio_set_dir(PINOD, GPIO_OUT);

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

     // Inicializa o buzzer
     gpio_init(BUZZER);
     gpio_set_function(BUZZER, GPIO_OUT);

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

    xTaskCreate(vTaskGerenciaHardware, "Hardware Task", configMINIMAL_STACK_SIZE + 256, NULL, tskIDLE_PRIORITY + 1, &xTaskGerenciaHardware);
    xTaskCreate(vTaskDisplay7Seg, "7-Seg Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &xTaskDisplay7Seg);
    xTaskCreate(vBuzzerTask, "Buzzer Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &xTaskBuzzer);
    xTaskCreate(vSemaforoControleNormal, "Semaforo Normal", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, &xTaskSemaforoNormal);
    xTaskCreate(vSemaforoControleNoturno, "Semaforo Noturno", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, &xTaskSemaforoNoturno);
    xTaskCreate(vTaskControleModo, "Controle Modos", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, &xTaskControleModo);

    vTaskSuspend(xTaskSemaforoNoturno);

    vTaskStartScheduler();
    
    panic_unsupported();
}
