# Infraestructura del proyecto — Detector de Fuga de GLP

Documento técnico que detalla **toda la arquitectura** del sistema: desde el sensor
físico hasta la visualización en el dashboard. Sirve como referencia para el informe
y la sustentación del curso de IoT (UNMSM).

---

## 1. Visión general

Nodo IoT basado en **ESP32-S3** que monitorea gas combustible (GLP) con un sensor
**MQ-2**. Ante una lectura de riesgo, ejecuta de forma **autónoma** el corte de energía
de un circuito simulado y activa un extractor mediante un módulo relay. El estado se
publica en tiempo real por **MQTT** (broker Mosquitto local) con **QoS diferenciado**
según la criticidad y mecanismo **LWT** para detectar caídas del nodo. Los datos se
almacenan en **InfluxDB** y se visualizan en **Node-RED** y **Grafana**.

**Principio de diseño clave:** el sistema tiene **dos caminos independientes**:

```
                     ┌─► CAMINO DE SEGURIDAD (local, instantáneo, NO necesita red)
[ESP32 detecta gas] ─┤     → corta relay + extractor + buzzer
                     │
                     └─► CAMINO DE DATOS (por WiFi, para monitoreo)
                           → MQTT → Mosquitto → Node-RED → InfluxDB → Grafana
```

El **corte de gas no depende de internet**: el ESP32 decide localmente en milisegundos.
El camino de datos solo sirve para **monitorear y registrar**.

---

## 2. Arquitectura general

```
┌───────────────────────────────────┐      WiFi 2.4 GHz       ┌──────────────────────────────────────────┐
│   NODO ESP32-S3  (C++ / PlatformIO)│      MQTT / TCP :1883   │   LAPTOP  (servidor, Docker)               │
│                                    │  ────────────────────►  │                                            │
│  Sensor:   MQ-2 (analógico, ADC)   │                         │   ┌──────────┐   ┌──────────┐              │
│  Actuadores: relay (corte+extractor)                         │   │Mosquitto │──►│ Node-RED │──┐          │
│              buzzer, LED RGB        │  ◄────────────────────  │   │ (broker) │   │(integra) │  │          │
│  Display:  OLED 0.96" (I2C)         │      comandos           │   └──────────┘   └──────────┘  ▼          │
│                                    │                         │                     ┌──────────┐          │
│  Lógica:  máquina de estados       │                         │                     │ InfluxDB │          │
│           (NORMAL/PRE-ALARMA/ALARMA)│                        │                     │(histórico)│         │
└───────────────────────────────────┘                         │                     └────┬─────┘          │
                                                               │                          ▼                │
                                                               │                     ┌──────────┐          │
                                                               │                     │ Grafana  │          │
                                                               │                     │(dashboard)│         │
                                                               │                     └──────────┘          │
                                                               └──────────────────────────────────────────┘
```

**Flujo de un dato de punta a punta:**

```
MQ-2 mide → ESP32 calcula ppm → WiFi → Mosquitto → Node-RED → InfluxDB → Grafana
```

---

## 3. Capas de la infraestructura

### Capa 1 — Hardware (mundo físico)
| Componente | Función | Conexión |
|---|---|---|
| **ESP32-S3 DevKitC-1** | Microcontrolador con WiFi | — |
| **Sensor MQ-2** | Detecta GLP (salida analógica) | AOUT → **divisor ÷2** → GPIO4 |
| **Relay 4ch** | Corte de energía + extractor | IN1→GPIO10, IN2→GPIO11 |
| **Buzzer** | Alarma sonora | GPIO12 |
| **LED RGB** | Semáforo de estado | GPIO13/14/21 (470Ω c/u) |
| **OLED 0.96"** | Estado local | I2C: SDA→GPIO8, SCL→GPIO9 |

> **Detalle crítico:** la salida analógica del MQ-2 llega a ~5 V y el ADC del ESP32
> aguanta 3.3 V. Se usa un **divisor de tensión ÷2** (2× 1KΩ) para proteger el pin.
> El firmware lo compensa con `MQ2_VOLTAGE_RESOLUTION = 6.6`.

### Capa 2 — Firmware (C++ / Arduino)
- **Entorno:** VS Code + PlatformIO. Framework Arduino.
- **Responsabilidad:** inteligencia local (lee sensor, decide, actúa) — funciona aunque
  se caiga el WiFi.
- **Componentes de código:**
  - Calibración **Rs/Ro** del MQ-2 (curva de GLP).
  - **Máquina de estados con histéresis**: NORMAL → PRE-ALARMA → ALARMA.
  - Actuadores (relay, buzzer, LED, OLED).
  - WiFi + mDNS con IP de respaldo.
  - Cliente MQTT con QoS y LWT.
- **Librerías** (`platformio.ini`):

| Librería | Uso |
|---|---|
| `256dpi/MQTT` | MQTT con QoS 0/1 + LWT (NO PubSubClient, que solo hace QoS 0) |
| `bblanchon/ArduinoJson` | Construir los payloads JSON |
| `miguel5612/MQUnifiedsensor` | Calibración Rs/Ro y cálculo de ppm |
| `adafruit/Adafruit SSD1306` + `GFX` | Pantalla OLED |

### Capa 3 — Red y protocolo
- **WiFi 2.4 GHz:** el ESP32 **no soporta 5 GHz**. Con hotspot de iPhone se activa
  "Maximizar compatibilidad" para forzar 2.4 GHz.
- **mDNS:** el ESP32 localiza la laptop por nombre (`<host>.local`) sin IP fija.
- **IP de respaldo:** si mDNS falla (los hotspots suelen bloquear multicast), el firmware
  cae a una IP fija. **Este respaldo fue clave** para conectar en el hotspot.

### Capa 4 — Broker MQTT (Mosquitto)
- Servidor de mensajería que **recibe y reparte** todos los mensajes (pub/sub).
- Gestiona QoS, mensajes retenidos y LWT.
- Corre en Docker, puerto **1883**.

### Capa 5 — Integración (Node-RED)
- Programación visual (nodos). Se suscribe a los topics, transforma el JSON y
  **escribe en InfluxDB**. Incluye un dashboard simple propio.
- **Es el puente** entre MQTT e InfluxDB (Mosquitto no habla con la base de datos).

### Capa 6 — Almacenamiento (InfluxDB)
- Base de datos **de series de tiempo** (optimizada para mediciones con marca de tiempo).
- Guarda el histórico de ppm y eventos de alarma.

### Capa 7 — Visualización (Grafana)
- **Lee** de InfluxDB y grafica (líneas de tiempo, umbrales de color, indicadores).
- Grafana no almacena datos; solo consulta a InfluxDB.

### Capa transversal — Contenedores (Docker)
- `docker-compose.yml` levanta los 4 servicios idénticos en cualquier máquina
  (reproducibilidad Mac ↔ Windows).

---

## 4. Diseño de la comunicación MQTT

| Topic | QoS | Retained | Contenido |
|---|---|---|---|
| `gas/nodo1/telemetria` | 0 | no | ppm periódicos (cada 3 s) |
| `gas/nodo1/alarma` | 1 | sí | evento crítico de fuga |
| `gas/nodo1/estado` | 1 | sí | `online` / `offline` (LWT) |
| `gas/nodo1/actuador` | 1 | no | estado de relays (corte/extractor) |

- **QoS 0** para telemetría: si se pierde un dato periódico, no importa.
- **QoS 1** para alarma y estado: deben llegar sí o sí.
- **Retained:** un suscriptor que llega tarde recibe de inmediato el último estado.
- **LWT (Last Will and Testament):** si el nodo se cae, Mosquitto publica `offline`
  automáticamente en `gas/nodo1/estado`.

---

## 5. Stack tecnológico

| Capa | Tecnología | Rol |
|---|---|---|
| Hardware | ESP32-S3 + MQ-2 | Mide y actúa físicamente |
| Firmware | C++ / Arduino / PlatformIO | Inteligencia local |
| Protocolo | MQTT | Mensajería IoT |
| Broker | Eclipse Mosquitto | Reparte los mensajes |
| Integración | Node-RED | Conecta MQTT ↔ InfluxDB |
| Almacenamiento | InfluxDB 2 | Histórico de series de tiempo |
| Visualización | Grafana | Dashboards |
| Contenedores | Docker Compose | Despliegue reproducible |
| Desarrollo | VS Code + PlatformIO + Git | Programar y versionar |

---

## 6. Estructura del repositorio

```
proyecto-gas-glp/
├── firmware/                  Firmware ESP32-S3 (PlatformIO, C++)
│   ├── platformio.ini
│   ├── include/
│   │   ├── config.h           pines, calibración, umbrales, topics
│   │   ├── secrets.example.h  plantilla (SÍ se sube)
│   │   └── secrets.h          datos privados (NO se sube)
│   └── src/main.cpp
├── server/                    Stack del servidor (Docker)
│   ├── docker-compose.yml
│   ├── mosquitto/config/mosquitto.conf
│   ├── nodered/flow-glp.json  flujo listo para importar
│   └── grafana-provision/dashboard-glp.json
├── scripts/
│   ├── setup-servidor.sh      provisiona InfluxDB + Node-RED + Grafana
│   └── simular-esp32.sh       simula al ESP32 publicando por MQTT
├── docs/
│   ├── CONEXIONES.md          pinout y montaje
│   ├── diagrama-conexiones.svg/png/html
│   └── INFRAESTRUCTURA.md     (este documento)
├── README.md
└── COMO-LEVANTAR.md           guía de puesta en marcha
```

---

## 7. Configuración de red

| Parámetro | Valor |
|---|---|
| Banda WiFi | **2.4 GHz** (obligatorio para ESP32) |
| Broker (mDNS) | `<nombre-laptop>.local` |
| Broker (respaldo) | IP fija de la laptop en el hotspot (`172.20.10.x`) |
| Puerto MQTT | 1883 |
| Puertos web | Node-RED 1880 · InfluxDB 8086 · Grafana 3000 |

---

## 8. Estado de verificación

Verificado con **hardware real** (ESP32-S3 físico publicando en vivo):

| Elemento | Estado |
|---|---|
| Entorno de desarrollo (offline) | ✅ |
| Firmware compila y flashea | ✅ |
| Firmware ejecutándose en la placa | ✅ |
| WiFi 2.4 GHz | ✅ |
| mDNS + IP de respaldo | ✅ |
| MQTT publicando al broker (estado, telemetría, LWT) | ✅ |
| Pipeline MQTT → Node-RED → InfluxDB → Grafana | ✅ (probado con simulación) |
| Cableado físico del MQ-2 y actuadores | ⏳ pendiente |
| Calibración del sensor | ⏳ pendiente |

---

## 9. Referencias

- Guía oficial ESP32-S3-DevKitC-1 (Espressif)
- Datasheet MQ-2 (curva de GLP)
- Documentación de Mosquitto, Node-RED, InfluxDB y Grafana
- MQTT v3.1.1 / v5 (QoS, retained, LWT)
