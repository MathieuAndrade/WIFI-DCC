/**
 * @author Mathieu Andrade
 * @date 2021-01-02
 * @details This sofware is used to control a DCC++ base station with a D1-Mini
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LiquidCrystal_I2C.h>
#include <LittleFS.h>

#include "config.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

String listFiles(String path) {
  File dir = LittleFS.open(path, "r");
  File file = dir.openNextFile();

  String json = "\"files\": [";

  while (file) {
    if (file.isDirectory()) {
      json += "{\"name\":\"" + String(file.name()) + "\",";
      json += listFiles(path + file.name() + "/");
      json += "},";
    } else {
      json += "{\"name\":\"" + String(file.name()) + "\",\"size\":" + String(file.size()) + "},";
    }

    file = dir.openNextFile();
  }

  dir.close();
  file.close();

  json = json.substring(0, json.length() - 1);
  json += "]";

  return json;
}

void printOnLcd(uint8_t line, String header, String msg, String footer = "") {
  lcd.setCursor(0, line);
  lcd.print("                    "); // Clear line
  lcd.setCursor(0, line);
  lcd.print(header);
  lcd.setCursor(header.length(), line);
  lcd.print(msg);
  lcd.setCursor(header.length() + msg.length(), line);
  lcd.print(footer);
}

void handleWebSocketMessage(AsyncWebSocketClient *client, char *payload) {
  if (payload[0] == '<') {
    DCCSerial.printf("%s\n", payload);
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo *)arg;

  switch (type) {
  case WS_EVT_CONNECT:
    printOnLcd(2, "Client connectee : ", String(client->id()));
    break;

  case WS_EVT_DISCONNECT:
    printOnLcd(2, "Client deconnectee : ", String(client->id()));
    break;

  case WS_EVT_DATA:
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      data[len] = 0;
      handleWebSocketMessage(client, ((char *)data));
      printOnLcd(2, "S: ", ((char *)data));
    }
    break;

  default:
    break;
  }
}

void readDCCSerial() {
  String DCCpp_text = "";

  if (DCCSerial.available()) {
    DCCpp_text = DCCSerial.readStringUntil('\n');
    DCCpp_text.remove(DCCpp_text.length() - 1); // remove 0x0D at the end

    if (DCCpp_text.length() > 1) {
      ws.textAll(DCCpp_text);
      DCCpp_text.remove(17);
      printOnLcd(3, "R: ", DCCpp_text);
    }
  }
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    // open the file on first call and store the file handle in the request object
    request->_tempFile = LittleFS.open("/" + filename, "w");
  }

  if (len) {
    // stream the incoming chunk to the opened file
    request->_tempFile.write(data, len);
  }

  if (final) {
    // close the file handle as the upload is now done
    request->_tempFile.close();
    request->send(200);
  }
}

void serveMainPage(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html.gz", "text/html");
  response->addHeader("Content-Encoding", "gzip");
  request->send(response);
};

void setup() {
  Serial.begin(115200);
  Serial.swap();

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  printOnLcd(0, NAME, VERSION);

  // Initialize SPIFFS
  if (!LittleFS.begin()) {
    printOnLcd(1, "Erreur de lecture de la mémoire, redemarrez la centrale svp", "");
    return;
  }

  // https://github.com/gilmaimon/ArduinoWebsockets/issues/90#issuecomment-1224361896
  if (!WiFi.config(local_IP, gateway, subnet)) {
    printOnLcd(1, "Echec de configuration de la connexion..", "");
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    printOnLcd(1, "Connexion..", "");
  }

  // Set header for clients origin
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  // Serve files in directory "/" when request url starts with "/"
  server.serveStatic("/", LittleFS, "/");

  // Routes are managed directly by client app
  server.on("/", HTTP_GET, serveMainPage);
  server.on("/dashboard", HTTP_GET, serveMainPage);
  server.on("/mobile", HTTP_GET, serveMainPage);
  server.on("/log", HTTP_GET, serveMainPage);

  // Route for file upload
  server.on(
      "/file", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("name", true) && request->hasParam("type", true)) {
          const char *fileName = request->getParam("name", true)->value().c_str();
          request->send(LittleFS, fileName, "application/octet-stream");
        } else {
          request->send(400, "text/plain", "ERROR: name and type params required");
        }
      },
      handleUpload);

  // Route for deleting a file
  server.on(
      "/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasArg("name")) {
          String fileName = request->arg("name");
          LittleFS.remove("/" + fileName);
          request->send(200);
        } else {
          request->send(400, "text/plain", "ERROR: name params required");
        }
      });

  // Route to get stats
  server.on("/stats", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{\"totalSize\": " + String(ESP.getFlashChipRealSize());
    json += ",\"hotspot\": \"" + String(ssid) + "\" ,\"rssi\": \"" + String(WiFi.RSSI(), DEC) + "\"";
    json += ",\"ip\": \"" + WiFi.localIP().toString() + "\",";
    json += listFiles("/");
    json += "}";

    request->send(200, "application/json", json);
  });

  // Start Websocket server
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // Start server
  printOnLcd(1, "Demarrage..", "");
  server.begin();

  // Print ESP Local IP Address
  printOnLcd(1, "IP: ", WiFi.localIP().toString());

  // Print the received power of the network
  printOnLcd(2, "RSSI: ", String(WiFi.RSSI(), DEC), " dBm");

  // Finally print ready
  printOnLcd(3, "E: ", "D1-Mini pret");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    ws.cleanupClients();
    readDCCSerial();
  }
}
