// mqtt_service.c
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/dns.h"
#include "lwip/apps/mqtt.h"

#include "mqtt_service.h"

// --- Estrutura de Estado Interna ---
typedef struct {
    mqtt_client_t* mqtt_client_inst;
    ip_addr_t mqtt_server_address;
    bool is_connected;
    const mqtt_config_t* user_config;
    mqtt_message_callback_t user_callback;
    char incoming_topic[128];
    char incoming_payload[1024];
    int payload_pos;
} mqtt_state_t;

static mqtt_state_t g_state = {0};

// --- Callbacks internos do LwIP ---

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {
    strncpy(g_state.incoming_topic, topic, sizeof(g_state.incoming_topic) - 1);
    g_state.incoming_topic[sizeof(g_state.incoming_topic) - 1] = '\0';
    g_state.payload_pos = 0;
}

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    if (g_state.payload_pos + len < sizeof(g_state.incoming_payload)) {
        memcpy(&g_state.incoming_payload[g_state.payload_pos], data, len);
        g_state.payload_pos += len;
    }

    if (flags & MQTT_DATA_FLAG_LAST) {
        g_state.incoming_payload[g_state.payload_pos] = '\0';
        if (g_state.user_callback) {
            g_state.user_callback(g_state.incoming_topic, g_state.incoming_payload);
        }
    }
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("MQTT conectado com sucesso!\n");
        g_state.is_connected = true;
        mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, arg);
    } else {
        printf("Falha na conexão MQTT, status: %d\n", status);
        g_state.is_connected = false;
    }
}

// --- Auxiliares internas ---
static void start_mqtt_client(void) {
    struct mqtt_connect_client_info_t ci = {0};
    ci.client_id = g_state.user_config->client_id;
    ci.client_user = g_state.user_config->username;
    ci.client_pass = g_state.user_config->password;
    ci.keep_alive = 60;

    g_state.mqtt_client_inst = mqtt_client_new();
    if (!g_state.mqtt_client_inst) {
        printf("Erro ao criar instância MQTT\n");
        return;
    }

    cyw43_arch_lwip_begin();
    mqtt_client_connect(g_state.mqtt_client_inst, &g_state.mqtt_server_address, 1883, mqtt_connection_cb, NULL, &ci);
    cyw43_arch_lwip_end();
}

static void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *arg) {
    if (ipaddr) {
        g_state.mqtt_server_address = *ipaddr;
        printf("Servidor MQTT encontrado: %s\n", ipaddr_ntoa(ipaddr));
        start_mqtt_client();
    } else {
        printf("Falha na resolução de DNS para o servidor MQTT\n");
    }
}

// --- Implementação da API ---
bool mqtt_service_connect(const mqtt_config_t* config) {
    if (cyw43_arch_init()) {
        printf("Falha ao inicializar CYW43\n");
        return false;
    }

    g_state.user_config = config;
    g_state.user_callback = config->on_message_callback;

    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(config->wifi_ssid, config->wifi_password, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return false;
    }
    printf("Conectado ao Wi-Fi: %s\n", config->wifi_ssid);

    cyw43_arch_lwip_begin();
    dns_gethostbyname(config->mqtt_server_host, &g_state.mqtt_server_address, dns_found_cb, NULL);
    cyw43_arch_lwip_end();

    return true;
}

bool mqtt_service_publish(const char* topic, const char* payload, int qos, bool retain) {
    if (!g_state.is_connected) {
        return false;
    }
    if (qos < 0 || qos > 2) qos = g_state.user_config->default_qos; // fallback
    cyw43_arch_lwip_begin();
    mqtt_publish(g_state.mqtt_client_inst, topic, payload, strlen(payload), qos, retain, NULL, NULL);
    cyw43_arch_lwip_end();
    return true;
}

bool mqtt_service_subscribe(const char* topic, int qos) {
    if (!g_state.is_connected) {
        return false;
    }
    if (qos < 0 || qos > 2) qos = g_state.user_config->default_qos; // fallback
    cyw43_arch_lwip_begin();
    mqtt_subscribe(g_state.mqtt_client_inst, topic, qos, NULL, NULL);
    cyw43_arch_lwip_end();
    return true;
}

bool mqtt_service_is_connected(void) {
    return g_state.is_connected;
}
