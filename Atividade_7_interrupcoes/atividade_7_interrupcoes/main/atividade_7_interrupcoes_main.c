#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_attr.h"

// Definição dos Pinos
#define LED_PIN       GPIO_NUM_12
#define BUTTON_PIN    GPIO_NUM_4

// Constantes de Tempo (microssegundos)
#define DEBOUNCE_TIME_US    (200 * 1000)       // 200 ms de janela para filtrar o bounce
#define TIMEOUT_LED_US      (10 * 1000 * 1000) // 10 segundos
#define LONG_PRESS_TIME_US  (2 * 1000 * 1000)  // 2 segundos

static esp_timer_handle_t led_timer_handle = NULL;
static esp_timer_handle_t long_press_timer_handle = NULL;
static volatile int64_t last_interrupt_time = 0;

// Callback: Executado após 10 segundos para apagar o LED
static void led_timer_callback(void* arg) {
    gpio_set_level(LED_PIN, 0);
}

// Callback: Executado 2 segundos após a interrupção
static void long_press_timer_callback(void* arg) {
    // Se após 2s o botão CONTINUAR pressionado (nível HIGH no pull-down)
    if (gpio_get_level(BUTTON_PIN) == 1) {
        gpio_set_level(LED_PIN, 0);         // Apaga o LED imediatamente
        esp_timer_stop(led_timer_handle);  // Cancela o timer de 10 segundos
    }
}

// ISR: Disparada APENAS ao pressionar o botão (Borda de Subida)
static void IRAM_ATTR button_isr_handler(void* arg) {
    int64_t current_time = esp_timer_get_time();

    // Debounce por software
    if ((current_time - last_interrupt_time) < DEBOUNCE_TIME_US) {
        return;
    }
    last_interrupt_time = current_time;

    // 1. Acende o LED
    gpio_set_level(LED_PIN, 1);

    // 2. Inicia / Reinicia o temporizador de 10 segundos
    esp_timer_stop(led_timer_handle);
    esp_timer_start_once(led_timer_handle, TIMEOUT_LED_US);

    // 3. Agenda a checagem de Pressionamento Longo (2s)
    esp_timer_stop(long_press_timer_handle);
    esp_timer_start_once(long_press_timer_handle, LONG_PRESS_TIME_US);
}

void app_main(void) {
    // Configuração do Pino do LED
    gpio_config_t io_conf_led = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf_led);
    gpio_set_level(LED_PIN, 0);

    // Configuração do Pino do Botão (Interrupção na Borda de Subida)
    gpio_config_t io_conf_button = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // Resistor físico de 10k external
        .intr_type = GPIO_INTR_POSEDGE          // Apenas ao pressionar (0 -> 1)
    };
    gpio_config(&io_conf_button);

    // Criação dos Timers
    const esp_timer_create_args_t led_timer_args = {
        .callback = &led_timer_callback,
        .name = "led_timeout"
    };
    esp_timer_create(&led_timer_args, &led_timer_handle);

    const esp_timer_create_args_t long_press_args = {
        .callback = &long_press_timer_callback,
        .name = "long_press"
    };
    esp_timer_create(&long_press_args, &long_press_timer_handle);

    // Instalação da ISR
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);

    // Bloqueia a task sem consumo de CPU
    while (1) {
        vTaskDelay(portMAX_DELAY);
    }
}