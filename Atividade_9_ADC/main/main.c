#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"

// Mapeamento de pinos compatível com placas de 30 pinos
#define POT_ADC_CHANNEL     ADC_CHANNEL_6      // GPIO34 (ADC1_CH6 - Entrada pura)
#define LED_PWM_GPIO        GPIO_NUM_25        // GPIO25 (Saída PWM)
#define BTN_HOLD_GPIO       GPIO_NUM_26        // GPIO26 (Entrada com Pull-Down)

#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_CHANNEL        LEDC_CHANNEL_0
#define LEDC_DUTY_RES       LEDC_TIMER_10_BIT  // Resolução 10-bit: 0 a 1023
#define LEDC_FREQUENCY      5000               // 5 kHz

typedef enum {
    MODE_LIVE = 0,
    MODE_HOLD
} system_mode_t;

static const char *TAG = "ATIVIDADE_09";

// Debounce não-bloqueante para botão Pull-Down (Ativo em HIGH)
static bool check_button_pressed(void) {
    static uint32_t last_debounce_time = 0;
    static int last_btn_state = 0;
    static int debounced_state = 0;
    
    int current_state = gpio_get_level(BTN_HOLD_GPIO);
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (current_state != last_btn_state) {
        last_debounce_time = now;
        last_btn_state = current_state;
    }

    if ((now - last_debounce_time) > 40) { 
        if (current_state != debounced_state) {
            debounced_state = current_state;
            // Transição para Nível ALTO (Pressionado = 1)
            if (debounced_state == 1) { 
                return true;
            }
        }
    }
    return false;
}

// Leitura do ADC com Oversampling (média de 8 amostras para eliminar ruídos)
static int read_adc_filtered(adc_oneshot_unit_handle_t handle) {
    int sum = 0;
    int raw_val = 0;
    for (int i = 0; i < 8; i++) {
        adc_oneshot_read(handle, POT_ADC_CHANNEL, &raw_val);
        sum += raw_val;
    }
    return sum / 8;
}

void app_main(void)
{
    // 1. Configuração do ADC (GPIO34)
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,           // 12 bits: 0 a 4095
        .atten = ADC_ATTEN_DB_12,              // Faixa até ~3.3V
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, POT_ADC_CHANNEL, &config));

    // 2. Configuração do Timer PWM (LEDC)
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 3. Configuração do Canal PWM (GPIO25)
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LED_PWM_GPIO,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    // 4. Configuração do GPIO do Botão (GPIO26)
    gpio_config_t btn_config = {
        .pin_bit_mask = (1ULL << BTN_HOLD_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,  // Pull-down habilitado
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_config);

    system_mode_t current_mode = MODE_LIVE;
    int adc_raw = 0;
    uint32_t voltage_mv = 0;
    uint32_t pwm_duty = 0;
    int last_applied_duty = -1;

    TickType_t last_telemetry_time = xTaskGetTickCount();

    while (1) {
        // 1. Verificação do Botão (LIVE <-> HOLD)
        if (check_button_pressed()) {
            current_mode = (current_mode == MODE_LIVE) ? MODE_HOLD : MODE_LIVE;
            ESP_LOGW(TAG, "Modo alterado para: %s", (current_mode == MODE_LIVE) ? "LIVE" : "HOLD");
        }

        // 2. Modo LIVE: Faz amostragem filtrada e atualiza hardware
        if (current_mode == MODE_LIVE) {
            adc_raw = read_adc_filtered(adc1_handle);

            // Filtro de corte inferior para garantir zeramento absoluto próximo ao GND
            if (adc_raw < 30) {
                adc_raw = 0;
                voltage_mv = 0;
                pwm_duty = 0;
            } else {
                voltage_mv = (adc_raw * 3300) / 4095;
                pwm_duty = (adc_raw * 1023) / 4095;
            }

            // Atualiza o PWM no GPIO25 apenas se o valor realmente mudar
            if ((int)pwm_duty != last_applied_duty) {
                last_applied_duty = pwm_duty;
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, pwm_duty);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
            }
        } 
        // No modo HOLD: Congela leituras do ADC e o duty cycle do LED.

        // 3. Telemetria Serial a cada 500 ms
        TickType_t now = xTaskGetTickCount();
        if ((now - last_telemetry_time) >= pdMS_TO_TICKS(500)) {
            last_telemetry_time = now;
            
            ESP_LOGI(TAG, "ADC Raw: %4d | Tensão: %4lu mV | Modo: %s", 
                     adc_raw, 
                     voltage_mv, 
                     (current_mode == MODE_LIVE) ? "LIVE" : "HOLD");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}