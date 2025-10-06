// =============================================================================
// Bibliotecas e Dependências
// =============================================================================
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "math.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "esp_adc/adc_oneshot.h"

// =============================================================================
// Definições e Constantes do Projeto
// =============================================================================
#define TAG_APP                 "PROJETO_TACO"
#define MOTOR_DRIVE_PIN         38
#define ADC_BLOCK_SIZE          50
#define PWM_FREQUENCY_HZ        1000
#define ADC_MAX_RAW_VALUE       4095.0
#define ADC_REFERENCE_VOLTAGE   2.9

// Configurações de Rede
#define WIFI_SSID       "kayky"
#define WIFI_PASSWORD   "kayky3212"
#define MQTT_BROKER_URI "mqtt://10.199.205.38:1884/mqtt"
#define MQTT_CLIENT_ID  "KAYKY_ESP32S3"

// Tópicos MQTT
#define MQTT_TOPIC_PUB_ADC      "ESP32/TACO_BLOCK"
#define MQTT_TOPIC_PUB_STATUS   "ESP32/INPUT"
#define MQTT_TOPIC_SUB_CMD      "ESP32/COMMAND"

// =============================================================================
// Tipos de Dados e Estruturas
// =============================================================================

// Estrutura para gerenciar um buffer duplo para as leituras do ADC
typedef struct {
    float amostras_a[ADC_BLOCK_SIZE];
    float amostras_b[ADC_BLOCK_SIZE];
    volatile bool buffer_a_pronto;
    volatile bool buffer_b_pronto;
    volatile float *buffer_ativo_escrita;
    volatile int indice_escrita;
} AdcDataManager;

// Estrutura para configurar um motor controlado por PWM
typedef struct {
    int pino_gpio;
    ledc_channel_t canal_ledc;
} MotorConfig;

// =============================================================================
// Variáveis Globais
// =============================================================================
static adc_oneshot_unit_handle_t g_adc_handle = NULL;
static esp_mqtt_client_handle_t  g_mqtt_client = NULL;
static adc_channel_t             g_adc_channel = ADC_CHANNEL_3;
static volatile bool             g_is_mqtt_online = false;

// Instâncias das estruturas
static AdcDataManager g_adc_buffer_manager;
static MotorConfig g_motor_principal = { .pino_gpio = MOTOR_DRIVE_PIN, .canal_ledc = LEDC_CHANNEL_0 };

// =============================================================================
// Protótipos de Funções
// =============================================================================
void motor_aplicar_sinal(MotorConfig motor, float duty_percent);

// =============================================================================
// Funções de Inicialização
// =============================================================================

// Inicia a estrutura do buffer duplo
void inicializar_buffer_duplo() {
    g_adc_buffer_manager.buffer_a_pronto = false;
    g_adc_buffer_manager.buffer_b_pronto = false;
    g_adc_buffer_manager.indice_escrita = 0;
    g_adc_buffer_manager.buffer_ativo_escrita = g_adc_buffer_manager.amostras_a;
}

// Configura o periférico ADC
void configurar_adc() {
    adc_oneshot_unit_init_cfg_t config_unidade = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&config_unidade, &g_adc_handle));

    adc_oneshot_chan_cfg_t config_canal = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(g_adc_handle, g_adc_channel, &config_canal));
}

// Configura o periférico LEDC para gerar PWM
void configurar_pwm(MotorConfig motor) {
    ledc_timer_config_t config_timer_pwm = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .freq_hz = PWM_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&config_timer_pwm));

    ledc_channel_config_t config_canal_pwm = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = motor.canal_ledc,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = motor.pino_gpio,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&config_canal_pwm));
}

// =============================================================================
// Funções de Lógica Principal
// =============================================================================

// Lê o valor do ADC e converte para tensão
float ler_tensao_adc() {
    int valor_raw = 0;
    adc_oneshot_read(g_adc_handle, g_adc_channel, &valor_raw);
    return (valor_raw / ADC_MAX_RAW_VALUE) * ADC_REFERENCE_VOLTAGE;
}

// Aplica um sinal de PWM (duty cycle em %) ao motor
void motor_aplicar_sinal(MotorConfig motor, float duty_percent) {
    uint32_t duty_abs = (uint32_t)(fabs(duty_percent) * ADC_MAX_RAW_VALUE / 100.0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, motor.canal_ledc, duty_abs);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, motor.canal_ledc);
}

// Constrói a string de dados e publica via MQTT
void publicar_bloco_adc(float buffer_dados[ADC_BLOCK_SIZE]) {
    // Aloca um buffer grande o suficiente (ex: 12 chars por float "-1.234," + null)
    char payload_string[ADC_BLOCK_SIZE * 12];
    char *p = payload_string; // Ponteiro para construir a string eficientemente

    for (int i = 0; i < ADC_BLOCK_SIZE; i++) {
        // 'sprintf' retorna o número de caracteres escritos
        int chars_escritos = sprintf(p, "%.3f", buffer_dados[i]);
        p += chars_escritos; // Avança o ponteiro
        if (i < (ADC_BLOCK_SIZE - 1)) {
            *p++ = ','; // Adiciona a vírgula e avança
        }
    }
    *p = '\0'; // Adiciona o terminador nulo

    if (g_mqtt_client != NULL && g_is_mqtt_online) {
        esp_mqtt_client_publish(g_mqtt_client, MQTT_TOPIC_PUB_ADC, payload_string, 0, 0, 0);
    }
}

// =============================================================================
// Lógica de Rede (Wi-Fi e MQTT)
// =============================================================================

// Handler para todos os eventos do cliente MQTT
static void mqtt_event_manager(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    g_mqtt_client = event->client;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG_APP, "MQTT Conectado com sucesso!");
            g_is_mqtt_online = true;
            esp_mqtt_client_subscribe(g_mqtt_client, MQTT_TOPIC_SUB_CMD, 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG_APP, "MQTT Desconectado.");
            g_is_mqtt_online = false;
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG_APP, "Inscrito no tópico com msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA: {
            char topico_recebido[128], dados_recebidos[128];
            snprintf(topico_recebido, event->topic_len + 1, "%.*s", event->topic_len, event->topic);
            snprintf(dados_recebidos, event->data_len + 1, "%.*s", event->data_len, event->data);
            
            ESP_LOGI(TAG_APP, "Recebido: Tópico [%s], Dado [%s]", topico_recebido, dados_recebidos);

            int duty_value = 0;
            // Tenta extrair o valor de duty da mensagem "SET_DUTY=valor"
            if (sscanf(dados_recebidos, "SET_DUTY=%d", &duty_value) == 1) {
                if (duty_value < 0) duty_value = 0;
                if (duty_value > 100) duty_value = 100;
                
                motor_aplicar_sinal(g_motor_principal, (float)duty_value);
                ESP_LOGI(TAG_APP, "Duty cycle ajustado para: %d%%", duty_value);

                char msg_status[32];
                snprintf(msg_status, sizeof(msg_status), "DUTY,%d", duty_value);
                esp_mqtt_client_publish(g_mqtt_client, MQTT_TOPIC_PUB_STATUS, msg_status, 0, 1, 0);
            }
            break;
        }
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG_APP, "Erro no MQTT!");
            g_is_mqtt_online = false;
            break;
        default:
            break;
    }
}

// Inicia a conexão Wi-Fi
void iniciar_wifi() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = { .ssid = WIFI_SSID, .password = WIFI_PASSWORD },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}

// Inicia o cliente MQTT
void iniciar_mqtt() {
    esp_mqtt_client_config_t config_mqtt = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.client_id = MQTT_CLIENT_ID,
        .session.protocol_ver = MQTT_PROTOCOL_V_3_1_1,
    };
    g_mqtt_client = esp_mqtt_client_init(&config_mqtt);
    esp_mqtt_client_register_event(g_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_manager, NULL);
    esp_mqtt_client_start(g_mqtt_client);
}

// =============================================================================
// Tarefas do FreeRTOS
// =============================================================================

// Tarefa que lê o ADC e preenche o buffer duplo
void tarefa_leitura_adc(void *pvParameters) {
    while (1) {
        // Pausa de 10ms, resultando em uma amostragem de ~100Hz
        vTaskDelay(pdMS_TO_TICKS(10));

        if (g_adc_buffer_manager.indice_escrita < ADC_BLOCK_SIZE) {
            g_adc_buffer_manager.buffer_ativo_escrita[g_adc_buffer_manager.indice_escrita] = ler_tensao_adc();
            g_adc_buffer_manager.indice_escrita++;
        } else { // Buffer atual está cheio, tenta trocar
            bool houve_perda_de_dados = false;
            
            if (g_adc_buffer_manager.buffer_ativo_escrita == g_adc_buffer_manager.amostras_a) {
                if (g_adc_buffer_manager.buffer_b_pronto == false) {
                    g_adc_buffer_manager.buffer_a_pronto = true;
                    g_adc_buffer_manager.buffer_ativo_escrita = g_adc_buffer_manager.amostras_b;
                    g_adc_buffer_manager.indice_escrita = 0;
                } else {
                    houve_perda_de_dados = true;
                }
            } else { // O buffer ativo era o B
                if (g_adc_buffer_manager.buffer_a_pronto == false) {
                    g_adc_buffer_manager.buffer_b_pronto = true;
                    g_adc_buffer_manager.buffer_ativo_escrita = g_adc_buffer_manager.amostras_a;
                    g_adc_buffer_manager.indice_escrita = 0;
                } else {
                    houve_perda_de_dados = true;
                }
            }
            
            if (houve_perda_de_dados) {
                ESP_LOGE(TAG_APP, "Buffer Overrun! A tarefa de publicação não está consumindo os dados rápido o suficiente. Descartando bloco.");
                g_adc_buffer_manager.indice_escrita = 0; // Reinicia a escrita no mesmo buffer
            }
        }
    }
}

// Tarefa que verifica os buffers e publica os dados via MQTT
void tarefa_publicacao_mqtt(void *pvParameters) {
    while (1) {
        if (g_adc_buffer_manager.buffer_a_pronto) {
            publicar_bloco_adc(g_adc_buffer_manager.amostras_a);
            g_adc_buffer_manager.buffer_a_pronto = false;
        }
        if (g_adc_buffer_manager.buffer_b_pronto) {
            publicar_bloco_adc(g_adc_buffer_manager.amostras_b);
            g_adc_buffer_manager.buffer_b_pronto = false;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// =============================================================================
// Função Principal (app_main)
// =============================================================================
void app_main() {
    // Inicializa o NVS (Non-Volatile Storage), necessário para o Wi-Fi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Inicia os serviços de rede
    iniciar_wifi();
    iniciar_mqtt();
    
    // Inicia os periféricos de hardware
    configurar_pwm(g_motor_principal);
    configurar_adc();
    
    // Prepara o buffer de dados
    inicializar_buffer_duplo();

    // Cria as tarefas que rodarão em paralelo
    xTaskCreate(tarefa_leitura_adc, "TarefaLeituraADC", 4096, NULL, 10, NULL);
    xTaskCreate(tarefa_publicacao_mqtt, "TarefaPublicacaoMQTT", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG_APP, "Sistema inicializado. Tarefas em execução.");
}