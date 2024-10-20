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

const char* ssid     = "Desktop_F6326123"; 
const char* password = "3490371103598201"; 
const char* server_host = "192.168.1.117"; 
const uint16_t websockets_server_port = 8080; 
const uint16_t mqtt_server_port = 1883; 

using namespace websockets; // Adicionando o namespace para facilitar o acesso
WebsocketsClient wsClient; // Cliente WebSocket
WiFiClient espClient;      // Cliente para o MQTT
PubSubClient mqttClient(espClient); // Cliente MQTT

// Callback para mensagens recebidas pelo WebSocket
void onMessageCallback(WebsocketsMessage message) {
    Serial.print("Received message: ");
    Serial.println(message.data());
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
void connect_websocket() {
    Serial.println("Connecting to WebSocket...");
    wsClient.onMessage(onMessageCallback);
    if (!wsClient.connect(server_host, websockets_server_port, "/ws")) {
        Serial.println("WebSocket connection failed!");
    } else {
        Serial.println("WebSocket connected.");
        wsClient.send("Hello from ESP32 camera stream!");
    }
}

// Conecta ao MQTT
void connect_mqtt() {
    mqttClient.setServer(server_host, mqtt_server_port);
    while (!mqttClient.connected()) {
        Serial.print("Connecting to MQTT...");
        if (mqttClient.connect("go-mqtt-client", "seu_usuario", "sua_senha")) {
            Serial.println("MQTT connected!");
            mqttClient.subscribe("test/topic");
        } else {
            Serial.print("MQTT connection failed, error: ");
            Serial.println(mqttClient.state());
            delay(2000);
        }
    }
}

// Função de configuração
void setup() {
    Serial.begin(115200);
    init_camera();
    init_wifi();
    connect_mqtt();      // Conectar ao MQTT primeiro
    connect_websocket();  // Em seguida, conectar ao WebSocket
}

// Loop principal
void loop() {
    // Assegura que o MQTT está conectado
    if (!mqttClient.connected()) {
        connect_mqtt();
    }
    mqttClient.loop();  // Lida com a comunicação MQTT

    wsClient.poll();    // Lida com a comunicação WebSocket

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
        wsClient.sendBinary((const char*) fb->buf, fb->len);
        esp_camera_fb_return(fb);
    } else {
        Serial.println("Image capture failed.");
    }
}
