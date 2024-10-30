#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <PubSubClient.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "driver/gpio.h"

// Configurações para a placa de câmera AI Thinker
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define PUBLISH_INTERVAL 30000  // Intervalo de 30 segundos

const char* ssid     = "VIVOFIBRA-A9A8"; 
const char* password = "A41168AE93"; 
const char* server_host = "192.168.15.117"; 
const uint16_t server_port = 8080; 
const uint16_t mqtt_server_port = 1883;
String chipid;
String MQTT_STATUS_TOPIC;
String MQTT_COMMAND_TOPIC;
String macAddress = WiFi.macAddress();

unsigned long lastPublishTime = 0; // Variável para armazenar o tempo da última publicação

using namespace websockets; // Adicionando o namespace para facilitar o acesso
WebsocketsClient wsClient; // Cliente WebSocket
bool startStreaming = false; // Variável para armazenar o estado da conexão
WiFiClient espClient;      // Cliente para o MQTT
PubSubClient mqttClient(espClient); // Cliente MQTT

void setup() {
    Serial.begin(115200);
    char chipIDBuffer[20];
    sprintf(chipIDBuffer, "%04X%08X", (uint16_t)(ESP.getEfuseMac() >> 32), (uint32_t)ESP.getEfuseMac());
    chipid = String(chipIDBuffer);
    MQTT_STATUS_TOPIC = "device/" + chipid + "/status";
    MQTT_COMMAND_TOPIC = "device/" + chipid + "/command";
    init_camera();
    init_wifi();
    send_device_details();
//    connect_mqtt();
    start_websocket();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
      init_wifi();
      }
    if (wsClient.available()) { // Verifica se há mensagens
        wsClient.poll(); // Escuta mensagens do servidor

    } else {
      start_websocket();  
    }

    if (startStreaming) {
        
        // Captura uma imagem da câmera
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            wsClient.sendBinary((const char*) fb->buf, fb->len);  // Envia os dados da câmera via WebSocket
            esp_camera_fb_return(fb);  // Libera o buffer da imagem
        } else {
            Serial.println("Falha ao capturar imagem.");
        }
    }
    
//    if (!mqttClient.connected()) {
//        connect_mqtt();  // Função para reconectar ao MQTT se desconectado
//    }
//    mqttClient.loop();  // Lida com a comunicação MQTT
//    pub_device_status();
    delay(100);
}

// Callback para mensagens recebidas pelo WebSocket
void onMessageCallback(WebsocketsMessage message) {
    Serial.println("Received: " + message.data());

    // Desconstrua o JSON
    StaticJsonDocument<200> doc; // Tamanho do documento JSON
    DeserializationError error = deserializeJson(doc, message.data());

    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    // Acesse os dados do JSON
    String device = doc["device"].as<String>(); // Obtém o valor de "device"
    int stream = doc["stream"];         // Obtém o valor de "stream"

    // Exibe os dados desconstruídos
    Serial.println("Device: " + device);
    Serial.println("Stream: " + String(stream));
    if(device == chipid) {
        if (stream == 1) {
            startStreaming = true;
            Serial.println("Iniciando o streaming.");
        } else {
            startStreaming = false;
            Serial.println("Parando o streaming.");
        }
    }
}

// Inicializa a câmera
esp_err_t init_camera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_VGA; 
    config.jpeg_quality = 15; 
    config.fb_count = 2;
  
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed: 0x%x\n", err);
        return err;
    }
    Serial.println("Camera initialized successfully.");
    return ESP_OK;
}

// Inicializa a conexão WiFi
esp_err_t init_wifi() {
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected.");
    return ESP_OK;
}

// Conecta ao WebSocket
void start_websocket() {
    Serial.println("Connecting to WebSocket...");
    if (!wsClient.connect(server_host, server_port, "/device/capture/" + chipid)) {
        Serial.println("WebSocket connection failed! (retrying in 1s)");
        delay(1000);
        start_websocket();
    } else {
        wsClient.onMessage(onMessageCallback);
        Serial.println("WebSocket connected.");
        wsClient.send("Connected: " + chipid);
    }
}


void stop_websocket() {
    Serial.println("Stopping WebSocket...");
    wsClient.close(); // Fecha a conexão WebSocket
    Serial.println("WebSocket disconnected.");
}

// Conecta ao MQTT
void connect_mqtt() {
    mqttClient.setServer(server_host, mqtt_server_port);
    mqttClient.setCallback(mqtt_callback);
    while (!mqttClient.connected()) {
        Serial.print("Connecting to MQTT...");
        if (mqttClient.connect("hl-mqtt-client")) {
            Serial.println("MQTT connected!");
            mqttClient.subscribe(MQTT_COMMAND_TOPIC.c_str());
        } else {
            Serial.print("MQTT connection failed, error: ");
            Serial.println(mqttClient.state());
            delay(2000);
        }
    }
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida no tópico: ");
  Serial.println(topic);

  // Converte o payload para uma string
  String jsonPayload = "";
  for (int i = 0; i < length; i++) {
    jsonPayload += (char)payload[i];
  }
  Serial.println("JSON recebido: " + jsonPayload);

  // Analisa o JSON
  StaticJsonDocument<256> doc; // Ajuste o tamanho conforme o JSON esperado
  DeserializationError error = deserializeJson(doc, jsonPayload);

  if (error) {
    Serial.print("Erro ao analisar JSON: ");
    Serial.println(error.c_str());
    return;
  }

  // Percorre o JSON dinamicamente
  for (JsonPair kv : doc.as<JsonObject>()) {
    const char* key = kv.key().c_str();         // Pega a chave
    const char* value = kv.value().as<const char*>(); // Pega o valor como string

    Serial.print("Chave: ");
    Serial.print(key);
    Serial.print(" | Valor: ");
    Serial.println(value);
    if(strcmp(key, "stream") == 0) {
      if(strcmp(value, "start") == 0) {
        start_websocket();
        } else {
          stop_websocket();
          }
      }
  }
  
}


void send_device_details() {
    StaticJsonDocument<200> doc;
    doc["id"] = chipid;  // Convertendo para string
    doc["device_type"] = "ESP32-CAM";  // Nome do dispositivo
    doc["status"] = "active";  // Status do dispositivo
    doc["mac_address"] = macAddress;

    // Serializando o JSON para uma string
    String jsonPayload;
    serializeJson(doc, jsonPayload);
        HTTPClient http;  // Cria um objeto HTTPClient
        
        String serverUrl = "http://" + String(server_host) + ":" + String(server_port) + "/device"; // URL do servidor
        
        http.begin(serverUrl);  // Especifica a URL do servidor
        http.addHeader("Content-Type", "application/json");  // Define o tipo de conteúdo como JSON
        
        int httpResponseCode = http.POST(jsonPayload);  // Envia a requisição POST com os dados JSON
        
        if (httpResponseCode > 0) {
            String response = http.getString();  // Obtém a resposta do servidor
            Serial.println("HTTP Response code: " + String(httpResponseCode));
            Serial.println("Response: " + response);
        } else {
            Serial.println("Erro na requisição POST: " + String(httpResponseCode));
        }
        http.end();  // Fecha a conexão HTTP
}

void pub_device_status() {
    // Publicação a cada 30 segundos
    if (millis() - lastPublishTime >= PUBLISH_INTERVAL) {
        lastPublishTime = millis(); // Atualiza o tempo da última publicação

        // Monta o JSON para envio
        StaticJsonDocument<200> doc;
        doc["id"] = chipid;
        doc["device_type"] = "ESP32-CAM";
        doc["status"] = "active";
        doc["timestamp"] = millis();
        doc["mac_address"] = macAddress;
        // Serializa o JSON para uma string
        String jsonPayload;
        serializeJson(doc, jsonPayload);

        // Publica o JSON no tópico MQTT
        mqttClient.publish(MQTT_STATUS_TOPIC.c_str(),jsonPayload.c_str());
        Serial.println("Publicado JSON no tópico MQTT: ");
        Serial.println(jsonPayload);
    }
}
