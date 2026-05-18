/*
===============================================================================
 ZNET HUB FINAL
 ESP32 BLE + BT CLASSIC + WIFI + TELNET + WATCHDOG
 COMPATIVEL COM ESP32 ARDUINO CORE 3.x
===============================================================================
*/

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

#include "BluetoothSerial.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include "esp_gap_ble_api.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "esp_task_wdt.h"
#include "esp_system.h"
#include "esp_chip_info.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ============================================================================
// WATCHDOG
// ============================================================================

#define WDT_TIMEOUT 15

// ============================================================================
// OLED
// ============================================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1);

// ============================================================================
// WIFI
// ============================================================================

const char* WIFI_SSID = "ZNet_HUB";
const char* WIFI_PASS = "12345678";

WiFiServer telnetServer(23);
WiFiClient telnetClient;

// ============================================================================
// BLUETOOTH CLASSIC
// ============================================================================

BluetoothSerial SerialBT;

// ============================================================================
// BLE
// ============================================================================

#define BLE_DEVICE_NAME "ZNet"

#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-1234-1234-1234-abcdef123456"

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;

bool bleConnected = false;

// ============================================================================
// LEDS
// ============================================================================

#define LED_BLE 17
#define LED_BT 18

// ============================================================================
// NODE STRUCT
// ============================================================================

#define MAX_NODES 20

struct PeerNode {

  String id;

  String transport;

  int rssi;

  unsigned long connectedAt;

  unsigned long lastSeen;

  bool active;
};

PeerNode nodes[MAX_NODES];

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================

void registrarNode(
  String id,
  String transport,
  int rssi);

void hardwareInfo();

void listarNodes();

void i2cscan();

void processCommand(String cmd);

void handleTelnet();

void atualizarDisplay();

void handleBluetoothClassic();

// ============================================================================
// BLE CALLBACKS
// ============================================================================

class MyServerCallbacks : public BLEServerCallbacks {

  void onConnect(BLEServer* pServer) {

    bleConnected = true;

    digitalWrite(LED_BLE, HIGH);

    Serial.println("BLE CONNECTED");

    registrarNode(
      "BLE_CLIENT",
      "BLE",
      -50);
  }

  void onDisconnect(BLEServer* pServer) {

    bleConnected = false;

    digitalWrite(LED_BLE, LOW);

    Serial.println("BLE DISCONNECTED");

    BLEDevice::startAdvertising();
  }
};

// ============================================================================
// REGISTER NODE
// ============================================================================

void registrarNode(
  String id,
  String transport,
  int rssi) {

  for (int i = 0; i < MAX_NODES; i++) {

    if (!nodes[i].active) {

      nodes[i].id = id;

      nodes[i].transport = transport;

      nodes[i].rssi = rssi;

      nodes[i].connectedAt = millis();

      nodes[i].lastSeen = millis();

      nodes[i].active = true;

      return;
    }
  }
}

// ============================================================================
// OLED DISPLAY
// ============================================================================

void atualizarDisplay() {

  display.clearDisplay();

  display.setTextSize(1);

  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);

  display.println("ZNet HUB");

  display.setCursor(0, 12);

  display.print("BLE: ");

  display.println(
    bleConnected ? "ON" : "OFF");

  display.setCursor(0, 24);

  display.print("BT: ");

  display.println(
    SerialBT.connected() ? "ON" : "OFF");

  display.setCursor(0, 36);

  display.print("HEAP:");

  display.println(
    ESP.getFreeHeap());

  display.setCursor(0, 48);

  display.print("WiFi:");

  display.println(WIFI_SSID);

  display.display();
}

// ============================================================================
// HARDWARE INFO
// ============================================================================

void hardwareInfo() {

  esp_chip_info_t chip_info;

  esp_chip_info(&chip_info);

  telnetClient.println("");
  telnetClient.println("===== HARDWARE =====");

  telnetClient.print("Chip: ");
  telnetClient.println(ESP.getChipModel());

  telnetClient.print("Revision: ");
  telnetClient.println(ESP.getChipRevision());

  telnetClient.print("CPU MHz: ");
  telnetClient.println(ESP.getCpuFreqMHz());

  telnetClient.print("CPU Cores: ");
  telnetClient.println(chip_info.cores);

  telnetClient.println("");

  telnetClient.println("RAM");

  telnetClient.print("Free Heap: ");
  telnetClient.println(ESP.getFreeHeap());

  telnetClient.print("Min Heap: ");
  telnetClient.println(ESP.getMinFreeHeap());

  telnetClient.print("Max Alloc Heap: ");
  telnetClient.println(ESP.getMaxAllocHeap());

  telnetClient.println("");

  telnetClient.println("WIFI");

  telnetClient.print("SSID: ");
  telnetClient.println(WIFI_SSID);

  telnetClient.print("IP: ");
  telnetClient.println(WiFi.softAPIP());

  telnetClient.print("Stations: ");
  telnetClient.println(WiFi.softAPgetStationNum());

  telnetClient.println("");

  telnetClient.println("BLUETOOTH");

  telnetClient.print("BLE: ");

  telnetClient.println(
    bleConnected ? "CONNECTED" : "DISCONNECTED");

  telnetClient.print("BT CLASSIC: ");

  telnetClient.println(
    SerialBT.connected() ? "CONNECTED" : "DISCONNECTED");

  telnetClient.println("");

  telnetClient.println("====================");
  telnetClient.println("");
}

// ============================================================================
// LIST NODES
// ============================================================================

void listarNodes() {

  telnetClient.println("");
  telnetClient.println("===== NODES =====");

  for (int i = 0; i < MAX_NODES; i++) {

    if (nodes[i].active) {

      telnetClient.print("ID: ");
      telnetClient.println(nodes[i].id);

      telnetClient.print("TRANSPORT: ");
      telnetClient.println(nodes[i].transport);

      telnetClient.print("RSSI: ");
      telnetClient.println(nodes[i].rssi);

      telnetClient.println("----------------");
    }
  }

  telnetClient.println("");
}

// ============================================================================
// I2C SCAN
// ============================================================================

void i2cscan() {

  telnetClient.println("");
  telnetClient.println("===== I2C SCAN =====");

  byte count = 0;

  for (byte i = 1; i < 127; i++) {

    Wire.beginTransmission(i);

    if (Wire.endTransmission() == 0) {

      telnetClient.print("FOUND: 0x");

      telnetClient.println(i, HEX);

      count++;
    }
  }

  telnetClient.print("TOTAL DEVICES: ");

  telnetClient.println(count);

  telnetClient.println("");
}

// ============================================================================
// PROCESS COMMAND
// ============================================================================

void processCommand(String cmd) {

  cmd.trim();

  cmd.toLowerCase();

  if (cmd == "help") {

    telnetClient.println("");
    telnetClient.println("===== ZNET COMMANDS =====");

    telnetClient.println("");

    telnetClient.println("help");
    telnetClient.println("hardware");
    telnetClient.println("stats");
    telnetClient.println("watchdog");
    telnetClient.println("heap");
    telnetClient.println("reboot");

    telnetClient.println("");

    telnetClient.println("list nodes");
    telnetClient.println("pinlist");
    telnetClient.println("i2cscan");

    telnetClient.println("");

    telnetClient.println("ble status");
    telnetClient.println("bt status");

    telnetClient.println("");
  }

  else if (cmd == "hardware") {

    hardwareInfo();
  }

  else if (cmd == "list nodes") {

    listarNodes();
  }

  else if (cmd == "pinlist") {

    telnetClient.println("");

    telnetClient.println("===== PIN LIST =====");

    telnetClient.println("");

    telnetClient.println("OLED");
    telnetClient.println("SDA -> GPIO 21");
    telnetClient.println("SCL -> GPIO 22");

    telnetClient.println("");

    telnetClient.println("BLE LED");
    telnetClient.println("GPIO 17");

    telnetClient.println("");

    telnetClient.println("BT LED");
    telnetClient.println("GPIO 18");

    telnetClient.println("");

    telnetClient.println("I2C DEVICES");
    telnetClient.println("INA219 -> SDA/SCL");
    telnetClient.println("BH1750 -> SDA/SCL");

    telnetClient.println("");
  }

  else if (cmd == "i2cscan") {

    i2cscan();
  }

  else if (cmd == "watchdog") {

    telnetClient.println("");

    telnetClient.print("WATCHDOG: ");

    telnetClient.print(WDT_TIMEOUT);

    telnetClient.println(" sec");

    telnetClient.println("");
  }

  else if (cmd == "stats") {

    telnetClient.println("");

    telnetClient.print("UPTIME(ms): ");

    telnetClient.println(millis());

    telnetClient.print("FREE HEAP: ");

    telnetClient.println(ESP.getFreeHeap());

    telnetClient.println("");
  }

  else if (cmd == "heap") {

    telnetClient.println("");

    telnetClient.print("FREE HEAP: ");

    telnetClient.println(ESP.getFreeHeap());

    telnetClient.print("MIN FREE HEAP: ");

    telnetClient.println(ESP.getMinFreeHeap());

    telnetClient.println("");
  }

  else if (cmd == "ble status") {

    telnetClient.println("");

    telnetClient.print("BLE: ");

    telnetClient.println(
      bleConnected ? "CONNECTED" : "DISCONNECTED");

    telnetClient.println("");
  }

  else if (cmd == "bt status") {

    telnetClient.println("");

    telnetClient.print("BT CLASSIC: ");

    telnetClient.println(
      SerialBT.connected() ? "CONNECTED" : "DISCONNECTED");

    telnetClient.println("");
  }

  else if (cmd == "reboot") {

    telnetClient.println("");

    telnetClient.println("REBOOTING...");

    delay(1000);

    ESP.restart();
  }

  else {

    telnetClient.println("");

    telnetClient.println("UNKNOWN COMMAND");

    telnetClient.println("");
  }

  telnetClient.print("ZNet> ");
}

// ============================================================================
// TELNET
// ============================================================================

void handleTelnet() {

  if (telnetServer.hasClient()) {

    if (
      !telnetClient || !telnetClient.connected()) {

      telnetClient = telnetServer.available();

      telnetClient.println("");
      telnetClient.println("================================");
      telnetClient.println("         ZNET HUB");
      telnetClient.println("================================");

      telnetClient.println("");

      telnetClient.println("TYPE: help");

      telnetClient.println("");

      telnetClient.print("ZNet> ");
    }

    else {

      WiFiClient rejectClient =
        telnetServer.available();

      rejectClient.stop();
    }
  }

  if (
    telnetClient && telnetClient.connected() && telnetClient.available()) {

    String cmd =
      telnetClient.readStringUntil('\n');

    processCommand(cmd);
  }
}

// ============================================================================
// BLUETOOTH CLASSIC
// ============================================================================

void handleBluetoothClassic() {

  if (SerialBT.connected()) {

    digitalWrite(LED_BT, HIGH);

  } else {

    digitalWrite(LED_BT, LOW);
  }
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {

  Serial.begin(115200);

  pinMode(LED_BLE, OUTPUT);

  pinMode(LED_BT, OUTPUT);

  digitalWrite(LED_BLE, LOW);

  digitalWrite(LED_BT, LOW);

  // =========================================================================
  // OLED
  // =========================================================================

  Wire.begin(21, 22);

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDR)) {

    Serial.println("OLED FAIL");
  }

  display.clearDisplay();

  display.display();

  // =========================================================================
  // WIFI AP
  // =========================================================================

  WiFi.mode(WIFI_AP);

  WiFi.softAP(
    WIFI_SSID,
    WIFI_PASS);

  telnetServer.begin();

  telnetServer.setNoDelay(true);

  // =========================================================================
  // BT CLASSIC
  // =========================================================================

  SerialBT.begin("ZNet");

  // =========================================================================
  // BLE
  // =========================================================================

  BLEDevice::init(BLE_DEVICE_NAME);

  esp_ble_tx_power_set(
    ESP_BLE_PWR_TYPE_DEFAULT,
    ESP_PWR_LVL_P9);

  pServer =
    BLEDevice::createServer();

  pServer->setCallbacks(
    new MyServerCallbacks());

  BLEService* pService =
    pServer->createService(
      SERVICE_UUID);

  pCharacteristic =
    pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

  pCharacteristic->addDescriptor(
    new BLE2902());

  pService->start();

  BLEAdvertising* pAdvertising =
    BLEDevice::getAdvertising();

  pAdvertising->addServiceUUID(
    SERVICE_UUID);

  pAdvertising->setScanResponse(true);

  BLEDevice::startAdvertising();

  // =========================================================================
  // WATCHDOG
  // =========================================================================

  esp_task_wdt_config_t wdt_config = {

    .timeout_ms = WDT_TIMEOUT * 1000,

    .idle_core_mask =
      (1 << portNUM_PROCESSORS) - 1,

    .trigger_panic = true
  };

  esp_task_wdt_init(&wdt_config);

  esp_task_wdt_add(NULL);

  // =========================================================================
  // NODE
  // =========================================================================

  registrarNode(
    "ZNET_HUB",
    "LOCAL",
    0);

  // =========================================================================
  // SERIAL
  // =========================================================================

  Serial.println("");
  Serial.println("================================");
  Serial.println("ZNET HUB STARTED");
  Serial.println("================================");

  Serial.print("IP: ");

  Serial.println(WiFi.softAPIP());

  Serial.println("");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {

  handleTelnet();

  handleBluetoothClassic();

  atualizarDisplay();

  // =========================================================================
  // LOW HEAP PROTECTION
  // =========================================================================

  if (ESP.getFreeHeap() < 20000) {

    Serial.println("LOW HEAP REBOOT");

    delay(1000);

    ESP.restart();
  }

  // =========================================================================
  // WATCHDOG RESET
  // =========================================================================

  esp_task_wdt_reset();

  vTaskDelay(
    100 / portTICK_PERIOD_MS);
}