#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <OneWire.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> 
#include <DallasTemperature.h>
const char* serverUrl = "http://Tu ip Aqui/api/lecturas"; 

// --- Configuración Sensor ---
#define ONE_WIRE_BUS 32
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

Preferences prefs;

//int deviceID = 0;
int fermentacion_id = 0; 


// ⚠️ CAMBIA ESTE PIN (RECOMENDADO)
const int RESET_PIN = 4;  // Usa GPIO 4 en vez de 0

bool shouldSaveConfig = false;

void saveConfigCallback() {
  Serial.println("Guardando configuración...");
  shouldSaveConfig = true;
}

void setup() {
  Serial.begin(115200);
  sensors.begin();
  delay(1000);

  pinMode(RESET_PIN, INPUT_PULLUP);

  // ===== Leer ID =====
  prefs.begin("config", true);
  fermentacion_id = prefs.getInt("fermentacion_id ", 0);
  prefs.end();

  Serial.println("ID actual: " + String(fermentacion_id));

  WiFiManager wm;

  // ===== RESET MANUAL =====
  if (digitalRead(RESET_PIN) == LOW) {
    Serial.println("Botón detectado, esperando confirmación...");

    delay(3000); // mantener presionado

    if (digitalRead(RESET_PIN) == LOW) {
      Serial.println("🔥 Borrando todo...");

      wm.resetSettings();

      prefs.begin("config", false);
      prefs.clear();
      prefs.end();

      delay(1000);
      ESP.restart();
    }
  }

  // ===== PARAMETRO ID =====
  char idString[10];
  sprintf(idString, "%d", fermentacion_id);

  WiFiManagerParameter custom_id("id", "ID del Lote", idString, 10);

  wm.addParameter(&custom_id);
  wm.setSaveConfigCallback(saveConfigCallback);

  // ===== 🔥 FORZAR PORTAL (DEBUG) =====
  // DESCOMENTA ESTA LINEA PARA PROBAR:
  // wm.startConfigPortal("ESP32_Config");

  // ===== AUTO CONNECT =====
  if (!wm.autoConnect("ESP32_Config")) {
    Serial.println("Error WiFi, reiniciando...");
    ESP.restart();
  }

  Serial.println("Conectado ✅");

  // ===== Guardar ID =====
  if (shouldSaveConfig) {
    int newID = String(custom_id.getValue()).toInt();

    if (newID != fermentacion_id) {
      prefs.begin("config", false);
      prefs.putInt("fermentacion_id", newID);
      prefs.end();

       fermentacion_id = newID;
      Serial.println("Nuevo ID: " + String(fermentacion_id));
    }
  }
}

int tempmosto(){


    sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);


  return tempC;



}

void loop() {

int tempC = tempmosto();


if (tempC != DEVICE_DISCONNECTED_C) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      
      // MEJORA 1: Reutilización de conexión y Timeout explícito
      http.setReuse(true);
      http.setTimeout(5000); // 5 segundos de espera máxima

      if (http.begin(serverUrl)) { 
        http.addHeader("Content-Type", "application/json");
          // 🔐 API KEY
     
        // MEJORA 2: Forzar el Host en el Header (Ayuda a Nginx a entender la ruta)
        http.addHeader("Host", "Tu ip aqui");

        StaticJsonDocument<200> doc;
        doc["fermentacion_id"] = fermentacion_id;
        doc["temperatura_mosto"] = tempC;
        doc["burbujas_por_minuto"] = 15;
        doc["ph"] = 7.0;

        String requestBody;
        serializeJson(doc, requestBody);

        Serial.print("Enviando a API: ");
        Serial.println(requestBody);

        int httpResponseCode = http.POST(requestBody);

        if (httpResponseCode > 0) {
          Serial.printf("Respuesta: %d\n", httpResponseCode);
          if (httpResponseCode == 201 || httpResponseCode == 200) {
            Serial.println("Datos guardados correctamente.");
          }
        } else {
          // MEJORA 3: Diagnóstico detallado del error
          Serial.printf("Error HTTP: %s\n", http.errorToString(httpResponseCode).c_str());
        }
        http.end();
      } else {
        Serial.println("Error: No se pudo establecer conexión con el servidor.");
      }
    }
  } else {
    Serial.println("Error: Sensor DS18B20 no detectado.");
  }

  delay(10000);
}








