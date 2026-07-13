#pragma once
// =============================================================================
//  PLANTILLA DE SECRETOS   ->  COPIA este archivo a  secrets.h  y llena tus datos
//
//      cp secrets.example.h secrets.h        (Mac/Linux)
//      copy secrets.example.h secrets.h      (Windows)
//
//  secrets.h esta en el .gitignore: NUNCA se sube al repositorio.
//  Cada integrante del equipo usa SUS propios datos (WiFi, nombre de laptop).
// =============================================================================

// ---- WiFi (usa el HOTSPOT del celular en 2.4 GHz) --------------------------
// iPhone: activa "Maximizar compatibilidad" para forzar 2.4 GHz.
// Android: en la zona WiFi elige banda 2.4 GHz. (El ESP32 NO ve el 5 GHz.)
#define WIFI_SSID   "MI_HOTSPOT"
#define WIFI_PASS   "MI_PASSWORD"

// ---- Broker MQTT (la laptop que corre Docker) ------------------------------
// mDNS: nombre de la laptop SIN el ".local".
//   Mac     -> Terminal:  scutil --get LocalHostName
//   Windows -> CMD:       hostname
#define BROKER_HOSTNAME     "laptop-jose"

// IP de respaldo por si mDNS falla (redes que bloquean multicast).
// Averigua la IP de la laptop dentro del hotspot:
//   Mac     -> ipconfig getifaddr en0
//   Windows -> ipconfig   (busca "Direccion IPv4")
// En hotspot de iPhone suele ser 172.20.10.x
#define BROKER_FALLBACK_IP  "172.20.10.2"

#define MQTT_PORT       1883
#define MQTT_CLIENT_ID  "esp32-gas-nodo1"

// Credenciales del broker. Si Mosquitto esta en modo anonimo (dev), dejalas
// vacias ("") y el codigo conecta igual.
#define MQTT_USER   ""
#define MQTT_PASS   ""
