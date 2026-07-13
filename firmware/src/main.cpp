// =============================================================================
//  SISTEMA INTELIGENTE DE DETECCION DE FUGA DE GLP CON CORTE AUTOMATICO
//  Nodo IoT - ESP32-S3 DevKitC-1
//
//  Flujo:  MQ-2 -> calibracion Rs/Ro -> maquina de estados (con histeresis)
//          -> actuadores (relay corte + extractor, buzzer, LED, OLED)
//          -> MQTT (QoS diferenciado + LWT) hacia Mosquitto en la laptop
//
//  Broker: se localiza por mDNS (nombre .local) con IP de respaldo.
// =============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <MQTT.h>                 // 256dpi/MQTT  (QoS 0/1 + LWT)
#include <ArduinoJson.h>
#include <MQUnifiedsensor.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "config.h"
#include "secrets.h"

// ---------------------------------------------------------------------------
//  OBJETOS GLOBALES
// ---------------------------------------------------------------------------
WiFiClient          net;
MQTTClient          mqtt(512);    // buffer de 512 bytes para los payloads JSON
Adafruit_SSD1306    oled(OLED_ANCHO, OLED_ALTO, &Wire, -1);
MQUnifiedsensor     MQ2("ESP32", MQ2_VOLTAGE_RESOLUTION, MQ2_ADC_BITS, PIN_MQ2_AO, "MQ-2");

enum Estado { NORMAL, PREALARMA, ALARMA };
Estado  estado = NORMAL;
float   ppm    = 0.0f;

unsigned long tUltimaLectura    = 0;
unsigned long tUltimaTelemetria = 0;

// ---------------------------------------------------------------------------
//  UTILIDADES DE ACTUADORES
// ---------------------------------------------------------------------------
const char* nombreEstado(Estado e) {
  switch (e) {
    case NORMAL:    return "NORMAL";
    case PREALARMA: return "PRE-ALARMA";
    case ALARMA:    return "ALARMA";
  }
  return "?";
}

// Enciende/apaga un relay respetando si es activo en bajo.
// encendido = true  ->  bobina energizada (rele "activado")
void setRelay(int pin, bool encendido) {
  bool nivel = RELAY_ACTIVO_EN_BAJO ? !encendido : encendido;
  digitalWrite(pin, nivel ? HIGH : LOW);
}

// LED RGB (catodo o anodo comun segun config.h)
void setLED(bool r, bool g, bool b) {
  bool inv = LED_ANODO_COMUN;
  digitalWrite(PIN_LED_R, (r ^ inv) ? HIGH : LOW);
  digitalWrite(PIN_LED_G, (g ^ inv) ? HIGH : LOW);
  digitalWrite(PIN_LED_B, (b ^ inv) ? HIGH : LOW);
}

void setBuzzer(bool on) {
  digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
}

// ---------------------------------------------------------------------------
//  OLED
// ---------------------------------------------------------------------------
void mostrarOLED() {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print("GLP  ");
  oled.print(WiFi.isConnected() ? "WiFi:OK" : "WiFi:--");
  oled.print(mqtt.connected() ? " MQTT:OK" : " MQTT:--");

  oled.setTextSize(2);
  oled.setCursor(0, 16);
  oled.print(nombreEstado(estado));

  oled.setTextSize(2);
  oled.setCursor(0, 42);
  oled.print((int)ppm);
  oled.setTextSize(1);
  oled.print(" ppm");

  oled.display();
}

void oledMensaje(const char* l1, const char* l2) {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 20);
  oled.println(l1);
  if (l2) oled.println(l2);
  oled.display();
}

// ---------------------------------------------------------------------------
//  MAQUINA DE ESTADOS  (con histeresis)
// ---------------------------------------------------------------------------
Estado evaluarEstado(float p, Estado actual) {
  switch (actual) {
    case NORMAL:
      if (p >= PPM_ALARMA_ON)    return ALARMA;
      if (p >= PPM_PREALARMA_ON) return PREALARMA;
      return NORMAL;
    case PREALARMA:
      if (p >= PPM_ALARMA_ON)     return ALARMA;
      if (p <  PPM_PREALARMA_OFF) return NORMAL;
      return PREALARMA;
    case ALARMA:
      if (p < PPM_ALARMA_OFF) return PREALARMA;
      return ALARMA;
  }
  return actual;
}

// Aplica los actuadores segun el estado.
// OJO en el corte: aqui "activar" el rele CH1 = CORTAR la energia.
// Ajusta la logica a tu cableado (NO/NC) si lo necesitas.
void aplicarActuadores(Estado e) {
  switch (e) {
    case NORMAL:
      setLED(false, true, false);              // verde
      setRelay(PIN_RELAY_CORTE, false);        // deja pasar la energia
      setRelay(PIN_RELAY_EXTRACTOR, false);    // extractor apagado
      setBuzzer(false);
      break;
    case PREALARMA:
      setLED(true, true, false);               // ambar (R+G)
      setRelay(PIN_RELAY_CORTE, false);        // aun no corta
      setRelay(PIN_RELAY_EXTRACTOR, true);     // ventila preventivamente
      setBuzzer(false);
      break;
    case ALARMA:
      setLED(true, false, false);              // rojo
      setRelay(PIN_RELAY_CORTE, true);         // CORTE AUTOMATICO
      setRelay(PIN_RELAY_EXTRACTOR, true);     // extractor encendido
      setBuzzer(true);                         // sirena
      break;
  }
}

// ---------------------------------------------------------------------------
//  PUBLICACIONES MQTT
// ---------------------------------------------------------------------------
void publicarTelemetria() {
  StaticJsonDocument<160> doc;
  doc["ppm"]    = (int)ppm;
  doc["estado"] = nombreEstado(estado);
  doc["rssi"]   = WiFi.RSSI();
  char buf[160];
  serializeJson(doc, buf);
  // QoS 0, no retenido: telemetria periodica
  mqtt.publish(TOPIC_TELEMETRIA, buf, false, 0);
}

void publicarAlarma(bool activa) {
  StaticJsonDocument<160> doc;
  doc["alarma"] = activa;
  doc["ppm"]    = (int)ppm;
  doc["estado"] = nombreEstado(estado);
  char buf[160];
  serializeJson(doc, buf);
  // QoS 1 + retained: evento critico, debe llegar y quedar disponible
  mqtt.publish(TOPIC_ALARMA, buf, true, 1);
}

void publicarActuadores() {
  StaticJsonDocument<128> doc;
  doc["corte"]     = (estado == ALARMA);
  doc["extractor"] = (estado != NORMAL);
  char buf[128];
  serializeJson(doc, buf);
  mqtt.publish(TOPIC_ACTUADOR, buf, false, 1);
}

// ---------------------------------------------------------------------------
//  RED:  WiFi  +  localizacion del broker por mDNS (con respaldo)
// ---------------------------------------------------------------------------
void conectarWiFi() {
  Serial.printf("Conectando a WiFi '%s' ...\n", WIFI_SSID);
  oledMensaje("Conectando WiFi...", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.isConnected())
    Serial.printf("WiFi OK. IP: %s\n", WiFi.localIP().toString().c_str());
  else
    Serial.println("WiFi FALLO (reintentara en loop).");
}

// Devuelve la IP del broker: primero intenta mDNS, si falla usa la de respaldo.
IPAddress resolverBroker() {
  IPAddress ip;
  Serial.printf("Buscando broker '%s.local' por mDNS...\n", BROKER_HOSTNAME);
  ip = MDNS.queryHost(BROKER_HOSTNAME, 2000);
  if (ip != IPAddress(0, 0, 0, 0)) {
    Serial.printf("Broker por mDNS: %s\n", ip.toString().c_str());
    return ip;
  }
  Serial.println("mDNS fallo -> usando IP de respaldo.");
  ip.fromString(BROKER_FALLBACK_IP);
  return ip;
}

void conectarMQTT() {
  if (!WiFi.isConnected()) return;

  IPAddress brokerIP = resolverBroker();
  mqtt.begin(brokerIP, MQTT_PORT, net);

  // LWT (testamento): si el nodo se cae, el broker publica "offline" solo.
  mqtt.setWill(TOPIC_ESTADO, "offline", true, 1);   // retained, QoS 1

  Serial.print("Conectando a MQTT");
  oledMensaje("Conectando MQTT...", brokerIP.toString().c_str());
  unsigned long t0 = millis();
  while (!mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS) && millis() - t0 < 8000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  if (mqtt.connected()) {
    Serial.println("MQTT conectado.");
    mqtt.publish(TOPIC_ESTADO, "online", true, 1);   // avisa que esta vivo
    publicarActuadores();
  } else {
    Serial.println("MQTT no conecto (reintentara).");
  }
}

// ---------------------------------------------------------------------------
//  CALIBRACION DEL MQ-2  (Ro en aire limpio)
// ---------------------------------------------------------------------------
void calibrarMQ2() {
  MQ2.setRegressionMethod(1);          // PPM = a*(Rs/Ro)^b
  MQ2.setA(MQ2_LPG_A);
  MQ2.setB(MQ2_LPG_B);
  MQ2.init();

  Serial.println("Calibrando Ro en aire limpio...");
  oledMensaje("Calibrando MQ-2", "Aire limpio...");
  float ro = 0;
  for (int i = 0; i < 10; i++) {
    MQ2.update();
    ro += MQ2.calibrate(MQ2_RATIO_CLEAN_AIR);
    delay(200);
  }
  ro /= 10.0f;
  MQ2.setR0(ro);
  Serial.printf("Ro = %.2f kOhm\n", ro);

  if (isinf(ro) || ro == 0)
    Serial.println("ADVERTENCIA: Ro invalido. Revisa cableado/divisor del MQ-2.");
}

// ---------------------------------------------------------------------------
//  SETUP
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);

  // Pines de salida
  pinMode(PIN_RELAY_CORTE, OUTPUT);
  pinMode(PIN_RELAY_EXTRACTOR, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  aplicarActuadores(NORMAL);           // arranca en estado seguro

  // ADC a rango completo (~0-3.3 V)
  analogSetPinAttenuation(PIN_MQ2_AO, ADC_11db);

  // OLED
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED no detectado (revisa I2C 0x3C).");
  }
  oled.clearDisplay();
  oled.display();

  // Warm-up del sensor (en cada arranque). El burn-in real es de 24-48 h.
  Serial.println("Calentando MQ-2...");
  unsigned long t0 = millis();
  while (millis() - t0 < CALENTAMIENTO_MS) {
    int s = (CALENTAMIENTO_MS - (millis() - t0)) / 1000;
    char l2[24];
    snprintf(l2, sizeof(l2), "Espera %d s", s);
    oledMensaje("Calentando MQ-2", l2);
    delay(500);
  }

  calibrarMQ2();
  conectarWiFi();
  if (WiFi.isConnected()) MDNS.begin("esp32-gas");   // el nodo se anuncia tambien
  conectarMQTT();

  aplicarActuadores(estado);
  mostrarOLED();
}

// ---------------------------------------------------------------------------
//  LOOP
// ---------------------------------------------------------------------------
void loop() {
  mqtt.loop();

  // Reconexiones no bloqueantes
  if (!WiFi.isConnected()) conectarWiFi();
  if (WiFi.isConnected() && !mqtt.connected()) conectarMQTT();

  unsigned long now = millis();

  // 1) Lectura del sensor + maquina de estados
  if (now - tUltimaLectura >= INTERVALO_LECTURA_MS) {
    tUltimaLectura = now;
    MQ2.update();
    ppm = MQ2.readSensor();
    if (ppm < 0 || isnan(ppm)) ppm = 0;

    Estado nuevo = evaluarEstado(ppm, estado);
    if (nuevo != estado) {
      Serial.printf("Estado: %s -> %s  (%d ppm)\n",
                    nombreEstado(estado), nombreEstado(nuevo), (int)ppm);
      estado = nuevo;
      aplicarActuadores(estado);
      publicarActuadores();
      if (estado == ALARMA) publicarAlarma(true);    // QoS 1
      if (estado == NORMAL) publicarAlarma(false);   // limpia la alarma
      mostrarOLED();
    }
  }

  // 2) Telemetria periodica (QoS 0)
  if (now - tUltimaTelemetria >= INTERVALO_TELEMETRIA_MS) {
    tUltimaTelemetria = now;
    if (mqtt.connected()) publicarTelemetria();
    Serial.printf("ppm=%d  estado=%s\n", (int)ppm, nombreEstado(estado));
    mostrarOLED();
  }
}
