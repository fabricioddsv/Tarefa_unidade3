// mqtt_service.h
#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include <stdbool.h>

/**
 * @brief Ponteiro de função para o callback que será chamado quando uma mensagem MQTT for recebida.
 * @param topic O tópico no qual a mensagem foi recebida.
 * @param payload O conteúdo (dados) da mensagem.
 */
typedef void (*mqtt_message_callback_t)(const char* topic, const char* payload);

/**
 * @brief Estrutura de configuração para o serviço MQTT.
 * Todos os campos devem ser preenchidos antes de chamar mqtt_service_connect.
 */
typedef struct {
    const char* wifi_ssid;
    const char* wifi_password;
    const char* mqtt_server_host;
    const char* client_id;
    const char* username;
    const char* password;
    mqtt_message_callback_t on_message_callback; // Callback de mensagens recebidas
    int default_qos; ///< Nível QoS padrão (0, 1 ou 2)
} mqtt_config_t;

/**
 * @brief Inicializa o Wi-Fi e conecta ao broker MQTT de forma não-bloqueante.
 * @param config Um ponteiro para a estrutura de configuração devidamente preenchida.
 * @return true se a inicialização foi bem-sucedida, false caso contrário.
 */
bool mqtt_service_connect(const mqtt_config_t* config);

/**
 * @brief Publica uma mensagem em um tópico MQTT.
 * @param topic O tópico para o qual publicar.
 * @param payload A mensagem a ser enviada.
 * @param qos Nível QoS (0, 1 ou 2).
 * @param retain Se a mensagem deve ser retida pelo broker.
 * @return true se a publicação foi enfileirada com sucesso, false se não estiver conectado.
 */
bool mqtt_service_publish(const char* topic, const char* payload, int qos, bool retain);

/**
 * @brief Inscreve o cliente em um tópico MQTT para receber mensagens.
 * @param topic O tópico no qual se inscrever.
 * @param qos Nível QoS (0, 1 ou 2).
 * @return true se a inscrição foi enfileirada com sucesso, false se não estiver conectado.
 */
bool mqtt_service_subscribe(const char* topic, int qos);

/**
 * @brief Verifica se o cliente MQTT está atualmente conectado ao broker.
 * @return true se conectado, false caso contrário.
 */
bool mqtt_service_is_connected(void);

#endif // MQTT_SERVICE_H
