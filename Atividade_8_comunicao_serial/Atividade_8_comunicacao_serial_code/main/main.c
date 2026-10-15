#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

// Definições de Pinos e UART
#define UART_NUM           UART_NUM_2
#define TXD_PIN            (GPIO_NUM_17)
#define RXD_PIN            (GPIO_NUM_16)
#define LED_PIN            (GPIO_NUM_2)

#define BUF_SIZE           (1024)
static const char *TAG = "UART_LOOPBACK";

// Configura e inicializa o pino do LED
void init_gpio(void) {
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0); // Inicia com o LED apagado
}

// Configura e instala o driver da UART2
void init_uart2(void) {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // 1. Configura os parâmetros da UART2
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));

    // 2. Define os pinos para TX e RX
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // 3. Instala o driver com buffer de recepção
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
}

void app_main(void) {
    init_gpio();
    init_uart2();

    uint8_t rx_buffer[BUF_SIZE];
    bool toggle_cmd = true; // Alterna entre enviar LIGAR e DESLIGAR

    ESP_LOGI(TAG, "Sistema de Loopback UART2 iniciado.");

    while (1) {
        // Define o comando a enviar neste ciclo
        const char *tx_message = toggle_cmd ? "LIGAR" : "DESLIGAR";
        toggle_cmd = !toggle_cmd;

        // 1. Transmite a mensagem pela UART2
        ESP_LOGI(TAG, "[UART0] Enviando pela UART2: '%s'", tx_message);
        uart_write_bytes(UART_NUM, tx_message, strlen(tx_message));

        // 2. Lê os dados recebidos pelo RX com timeout de 1000ms
        int len = uart_read_bytes(UART_NUM, rx_buffer, BUF_SIZE - 1, pdMS_TO_TICKS(1000));

        if (len > 0) {
            // Garante o caractere nulo de terminação de string (\0) para leitura segura
            rx_buffer[len] = '\0';

            ESP_LOGI(TAG, "[UART0] Recebido da UART2: '%s' (%d bytes)", (char *)rx_buffer, len);

            // 3. Compara o comando recebido para acionar o LED
            if (strcmp((char *)rx_buffer, "LIGAR") == 0) {
                gpio_set_level(LED_PIN, 1);

            } else if (strcmp((char *)rx_buffer, "DESLIGAR") == 0) {
                gpio_set_level(LED_PIN, 0);

            } else {
                ESP_LOGW(TAG, "-> Comando desconhecido recebido.");
            }
        } else {
            ESP_LOGE(TAG, "Nenhum dado recebido (Verifique se o jumper entre TX e RX está conectado).");
        }

        // Aguarda 2 segundos até o próximo envio
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
