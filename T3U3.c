#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "mpu6050_handler.h"
#include "pico/cyw43_arch.h"
#include "mqtt_service.h" // Inclui nossa biblioteca MQTT
#include "hardware/rtc.h"
#include "lwip/netif.h"    
#include "lwip/ip4_addr.h" 
#include "ntp_client.h"

#define I2C_PORT i2c0
#define I2C_SDA  0   // Pino GPIO4 = SDA (ajuste conforme sua ligação)
#define I2C_SCL  1   // Pino GPIO5 = SCL (ajuste conforme sua ligação)

// --- Credenciais da Rede Wi-Fi ---
#define WIFI_SSID       "PRETA_INFOWAY"       // Nome da sua rede Wi-Fi (SSID)
#define WIFI_PASSWORD   "dpsouza1"      // Senha da sua rede Wi-Fi

// --- Configurações do Broker MQTT ---
#define MQTT_SERVER     "mqtt.iot.natal.br"  // Endereço IP ou hostname do seu broker MQTT
#define MQTT_CLIENT_ID  "pico_w_client_fabricio.silva"    // ID único para este cliente. Mude se tiver mais Picos.
#define MQTT_USERNAME   "desafio15"    // Usuário para autenticação no broker (deixe "" se não usar)
#define MQTT_PASSWORD   "desafio15.laica"      // Senha para autenticação no broker (deixe "" se não usar)
#define MQTT_QOS        0                     // Nível de Qualidade de Serviço padrão (0, 1 ou 2)

// --- Tópicos MQTT ---
// Tópico para receber comandos (ex: controlar o LED)
#define MQTT_TOPIC_COMMAND "ha/desafio15/fabricio.silva/set"

// Tópico para publicar status (ex: enviar um timestamp ou leitura de sensor)
#define MQTT_TOPIC_STATUS  "ha/desafio15/fabricio.silva/mpu6050"

#define NTP_SERVER          "pool.ntp.br" // Melhor usar o pool brasileiro
#define NTP_TIMEOUT_MS      5000
#define NTP_RESYNC_INTERVAL_MS 3600000 // Ressincronizar a cada 1 hora (3600 * 1000)

// Fuso horário de Brasília (UTC-3)
#define BRASILIA_OFFSET_SECONDS (-3 * 3600)

// 2. Variáveis com os dados que você quer inserir no JSON
const char* team = "desafio15";
const char* device = "bitdoglab01_fabricio.silva";
const char* ssid = WIFI_SSID;
const char* sensor = "MPU-6050";

void on_message_received(const char* topic, const char* payload);

int main() {
    stdio_init_all();
    sleep_ms(2000); // espera inicial para abrir monitor serial

    printf("Iniciando I2C...\n");

    // Inicializa I2C a 400kHz
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    printf("Iniciando MPU6050...\n");

    if (!mpu6050_init(I2C_PORT, GYRO_FS_250_DPS, ACCEL_FS_2G)) {
        printf("Falha ao inicializar MPU6050!\n");
        while (1) tight_loop_contents();
    }

    printf("MPU6050 inicializado com sucesso!\n");

    mpu6050_data_t data;

    // 1. Configurar a biblioteca MQTT
    mqtt_config_t config = {
        .wifi_ssid = WIFI_SSID,
        .wifi_password = WIFI_PASSWORD,
        .mqtt_server_host = MQTT_SERVER,
        .client_id = MQTT_CLIENT_ID,
        .username = MQTT_USERNAME,
        .password = MQTT_PASSWORD,
        .on_message_callback = on_message_received,
        .default_qos = MQTT_QOS
    };

    // 2. Tentar conectar
    while (!mqtt_service_connect(&config)) {
        printf("Erro ao iniciar o serviço MQTT. Tentando novamente...\n");
        sleep_ms(3000);
    }
    printf("Serviço MQTT iniciado. Aguardando conexão...\n");

    char ip_address_str[16];
    strncpy(ip_address_str, ip4addr_ntoa(netif_ip4_addr(netif_default)), sizeof(ip_address_str));
    printf("IP da Placa: %s\n", ip_address_str);

    // --- Variáveis para o relógio local (CORRIGIDAS) ---
    bool time_synchronized = false;
    time_t last_ntp_utc_time = 0;
    absolute_time_t last_ntp_sync_time; // <-- CORREÇÃO 1: Tipo correto
    uint32_t last_ntp_resync_ms = 0;     // <-- CORREÇÃO 3: Nova variável para resync

    // --- Variáveis de publicação MQTT ---
    uint32_t last_publish_time = 0;
    bool subscribed = false;

    // --- Loop de Sincronização Inicial (CORRIGIDO) ---
    printf("Obtendo hora NTP inicial...\n");
    while (!time_synchronized) {
        if (ntp_get_time(NTP_SERVER, NTP_TIMEOUT_MS)) {
            last_ntp_utc_time = ntp_get_last_time();
            last_ntp_sync_time = get_absolute_time();
            last_ntp_resync_ms = to_ms_since_boot(get_absolute_time()); // <-- CORREÇÃO 3
            time_synchronized = true;
            printf("Hora NTP obtida com sucesso!\n");
            break;
        } else {
            printf("Falha ao obter hora NTP. Tentando novamente em 5 segundos...\n");
            sleep_ms(5000);
        }
    }

    while (true) {
        // Essencial para a pilha de rede (Wi-Fi + MQTT)
        cyw43_arch_poll();

        // CALCULA A HORA ATUAL COM PRECISÃO
        uint64_t diff_us = absolute_time_diff_us(last_ntp_sync_time, get_absolute_time());
        time_t current_utc_time = last_ntp_utc_time + (diff_us / 1000000);

        // 1. Verifica se é hora de ressincronizar com o servidor NTP
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        if (now_ms - last_ntp_resync_ms > NTP_RESYNC_INTERVAL_MS) {
            printf("\n--- Ressincronizando com o NTP ---\n");
            if (ntp_get_time(NTP_SERVER, NTP_TIMEOUT_MS)) {
                last_ntp_utc_time = ntp_get_last_time();
                last_ntp_sync_time = get_absolute_time();
                last_ntp_resync_ms = now_ms;
                printf("Ressincronização bem-sucedida.\n");
            } else {
                printf("Falha na ressincronização. Usando relógio local.\n");
            }
        }
        
        // 2. Calcula a hora local (Brasília)
        time_t brasili_time = current_utc_time + BRASILIA_OFFSET_SECONDS;
        struct tm *local_tm = gmtime(&brasili_time);


        if (mqtt_service_is_connected()) {
            // Se está conectado, tenta se inscrever se ainda não o fez
            if (!subscribed) {
                if (mqtt_service_subscribe(MQTT_TOPIC_COMMAND, MQTT_QOS)) {
                    printf("Inscrito em '%s' com QoS %d\n", MQTT_TOPIC_COMMAND, MQTT_QOS);
                    subscribed = true;
                }
            } 
            
            // Publica a cada 10 segundos
            if (now_ms - last_publish_time > 10000) {
                last_publish_time = now_ms;
                
                static char message[512];

                
                if (mpu6050_read_data(&data)) {
                    char timestamp[20]; 
                    snprintf(timestamp, sizeof(timestamp),
                            "%04d-%02d-%02dT%02d:%02d:%02d",
                            local_tm->tm_year + 1900, local_tm->tm_mon + 1, local_tm->tm_mday,
                            local_tm->tm_hour, local_tm->tm_min, local_tm->tm_sec);
                    
                    snprintf(message, sizeof(message),
                            "{\"team\":\"%s\",\"device\":\"%s\",\"ip\":\"%s\",\"ssid\":\"%s\",\"sensor\":\"%s\","
                            "\"data\":{\"accel\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},"
                            "\"gyro\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},"
                            "\"temperature\":%.1f},"
                            "\"timestamp\":\"%s\"}",
                            team, device, ip_address_str, ssid, sensor,
                            data.accel_x, data.accel_y, data.accel_z,
                            data.gyro_x, data.gyro_y, data.gyro_z,
                            data.temperature, timestamp);

                    printf("Publicando %s em '%s'...\n", message, MQTT_TOPIC_STATUS);

                    if (!mqtt_service_publish(MQTT_TOPIC_STATUS, message, MQTT_QOS, false)){
                        printf("Falha ao tentar publicar mensagem.\n");
                    } else {
                        printf("Mensagem enviada para a fila de publicação.\n");
                    }
                } else {
                    printf("Falha na leitura do MPU6050, publicação cancelada.\n");
                }
            }
        } else {
            // Se a conexão MQTT caiu, reseta a flag para se inscrever novamente ao reconectar
            subscribed = false;
        }

        // Um pequeno sleep para a CPU "respirar", não afeta a precisão do tempo.
        sleep_ms(100); 
    } 

    return 0;
}

// Callback chamado quando chega uma mensagem MQTT
void on_message_received(const char* topic, const char* payload) {
    printf("Mensagem recebida! Tópico: %s, Payload: %s\n", topic, payload);

    // Exemplo: ligar/desligar o LED da Pico W
    if (strcmp(topic, MQTT_TOPIC_COMMAND) == 0) {
        if (strcmp(payload, "ON") == 0) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        } else if (strcmp(payload, "OFF") == 0) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        }
    }
}