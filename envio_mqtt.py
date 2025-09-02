import paho.mqtt.client as mqtt
import json
import time
import random
from datetime import datetime

# --- 1. Configurações do Broker MQTT e Credenciais ---
broker_address = "mqtt.iot.natal.br"
port = 1883
topic = "ha/desafio15/fabricio.silva/mpu6050"

# !!! IMPORTANTE: Substitua com suas credenciais reais !!!
username = "desafio15"
password = "desafio15.laica"

# --- 2. Função de Callback para Conexão ---
# Esta função é chamada quando o cliente se conecta ao broker.
def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print("Conectado ao Broker MQTT com sucesso!")
    else:
        print(f"Falha na conexão, código de retorno: {rc}")
        if rc == 5:
            print("Verifique se o username e a senha estão corretos.")
        

# --- 3. Inicialização do Cliente MQTT ---
# Criamos uma instância do cliente.
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, "python_publisher_fabricio")

# Define as credenciais de username e password
client.username_pw_set(username, password)

# Define a função de callback para o evento de conexão
client.on_connect = on_connect 

# Tenta conectar ao broker
try:
    print(f"Conectando ao broker {broker_address}...")
    client.connect(broker_address, port, 60)
except Exception as e:
    print(f"Não foi possível conectar ao broker: {e}")
    exit()

# Inicia o loop de rede em uma thread separada para processar mensagens
client.loop_start()

# --- 4. Loop Principal para Envio de Dados ---
try:
    # Aguarda um pouco para garantir que a conexão foi estabelecida
    time.sleep(2) 
    
    while True:
        # --- Simulação de Dados Variáveis ---
        # Gera valores aleatórios para simular as leituras do sensor MPU-6050
        accel_x = round(random.uniform(-10.0, 10.0), 2)
        accel_y = round(random.uniform(-10.0, 10.0), 2)
        accel_z = round(random.uniform(-10.0, 10.0), 2)

        gyro_x = round(random.uniform(-5.0, 5.0), 2)
        gyro_y = round(random.uniform(-5.0, 5.0), 2)
        gyro_z = round(random.uniform(-5.0, 5.0), 2)

        temperature = round(random.uniform(25.0, 50.0), 1)

        # Obtém o timestamp atual no formato ISO 8601
        # Usando a data atual, conforme o exemplo original, não 2025.
        current_timestamp = datetime.now().isoformat(timespec='seconds')

        # --- Montagem da Estrutura de Dados (Payload) ---
        payload = {
            "team": "desafio15",
            "device": "bitdoglab01_fabricio.silva",
            "ip": "192.168.0.101",
            "ssid": "tarefa-mqtt",
            "sensor": "MPU-6050",
            "data": {
                "accel": {
                    "x": accel_x,
                    "y": accel_y,
                    "z": accel_z
                },
                "gyro": {
                    "x": gyro_x,
                    "y": gyro_y,
                    "z": gyro_z
                },
                "temperature": temperature
            },
            "timestamp": current_timestamp
        }

        # --- Conversão e Publicação ---
        json_payload = json.dumps(payload, indent=4)

        result = client.publish(topic, json_payload)
        
        status = result[0]
        if status == 0:
            print(f"--- Publicado no tópico '{topic}' ---")
            print(json_payload)
        else:
            print(f"Falha ao enviar mensagem para o tópico '{topic}'")

        # --- Intervalo de Tempo ---
        time.sleep(10)

except KeyboardInterrupt:
    print("\nPublicação interrompida pelo usuário.")

finally:
    # Para o loop de rede e desconecta de forma limpa
    client.loop_stop()
    client.disconnect()
    print("Desconectado do broker MQTT.")
