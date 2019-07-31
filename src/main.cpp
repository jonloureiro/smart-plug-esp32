#include <stdio.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include "esp_system.h"


void setupWiFi();
void setupToken();
void setupWebSocket();
void setupWatchdogTimer();
void setupTimer();


const char *ssid     = "WIFISSI";
const char *password = "WIFIPASS";

const char *host     = "jonloureiro-smartplug.herokuapp.com"; // your ip: 192.168.x.x
const uint16_t port  = 80; // 8080
const char *url      = "/ws";
const char *protocol = "jonloureiro.dev";
const char *user     = "user";
const char *pass     = "password";
const char *agent    = "user-agent: Project/0.1";
const char *origin   = "origin: project://id";


hw_timer_t *watchdogTimer = NULL;
hw_timer_t *apiTimer      = NULL;
portMUX_TYPE apiMux       = portMUX_INITIALIZER_UNLOCKED;

volatile SemaphoreHandle_t apiSemaphore;
volatile uint8_t count;

uint8_t countAux;
uint32_t current;
char current_str[5];
bool connected;

WebSocketsClient webSocket;


void setup() {
  Serial.begin(115200);

  apiSemaphore = xSemaphoreCreateBinary();
  count = 0;
  current = 0;
  connected = false;

  delay(1000);

  setupWiFi();
  setupWebSocket();
  setupWatchdogTimer();
  setupTimer();
}


void loop() {
    Serial.print(".");
    timerWrite(watchdogTimer, 0);
    webSocket.loop();
    if (xSemaphoreTake(apiSemaphore, 0) == pdTRUE) {
        portENTER_CRITICAL(&apiMux);
        countAux = count;
        portEXIT_CRITICAL(&apiMux);
        if (countAux < 10)
            current += analogRead(36);
        else {
            portENTER_CRITICAL(&apiMux);
            count = 0;
            portEXIT_CRITICAL(&apiMux);
            current += analogRead(36);
            current /= countAux;
            if (connected) {
                sprintf(current_str, "%d", current);
                webSocket.sendTXT(current_str);
                Serial.printf("\nCorrente %s enviada!\n", current_str);
                current = 0;
            } // ELSE TRATAR
        }
    }
    delay(200);
}


void IRAM_ATTR resetModule() {
    ets_printf("Watchdog Timer: reboot...\n");
    esp_restart();
}


void IRAM_ATTR apiRequest() {
    portENTER_CRITICAL_ISR(&apiMux);
    count++;
    portEXIT_CRITICAL_ISR(&apiMux);
    xSemaphoreGiveFromISR(apiSemaphore, NULL);
}


void setupWatchdogTimer() {
    watchdogTimer = timerBegin(3, 80, true);
    timerAttachInterrupt(watchdogTimer, &resetModule, true);
    timerAlarmWrite(watchdogTimer, 5000000, false);
    timerAlarmEnable(watchdogTimer);
}


void setupTimer() {
    apiTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(apiTimer, &apiRequest, true);
    timerAlarmWrite(apiTimer, 1000000, true);
    timerAlarmEnable(apiTimer);
}


void setupWiFi() {
  Serial.print("\nAttempting to connect to SSID ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);

  for (uint8_t i = 0; i < 5; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected to WiFi");
      return;
    }
    Serial.print(".");
    delay(500);
  }

  setupWiFi();
}


// void hexdump(const void *mem, uint32_t len, uint8_t cols = 16) {
//     const uint8_t* src = (const uint8_t*) mem;
//     Serial.printf("\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
//     for(uint32_t i = 0; i < len; i++) {
//         if(i % cols == 0) {
//             Serial.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
//         }
//         Serial.printf("%02X ", *src);
//         src++;
//     }
//     Serial.printf("\n");
// }


void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            connected = false;
            Serial.printf("\nDisconnected!\n");
            break;

        case WStype_CONNECTED:
            connected = true;
            Serial.printf("\nConnected to url: %s\n", payload);
            break;

        case WStype_TEXT:
            Serial.printf("\nGet text: %s\n", payload);
            break;

        case WStype_BIN:
            Serial.printf("\nGet binary length: %u\n", length);
            // hexdump(payload, length);
            break;

        case WStype_ERROR:
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
        default:
            return;
    }
}

void setupWebSocket() {
    webSocket.begin(host, port, url, protocol);
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000);
    webSocket.enableHeartbeat(5000, 5000, 1);
    webSocket.setAuthorization(user, pass);
    webSocket.setExtraHeaders(agent);
    webSocket.setExtraHeaders(origin);
}
