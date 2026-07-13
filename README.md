# Sistema Inteligente de Detección de Fuga de Gas GLP con Corte Automático

Nodo IoT basado en **ESP32-S3** que monitorea gas combustible (GLP) con un sensor
**MQ-2** calibrado experimentalmente. Ante una lectura de riesgo ejecuta de forma
autónoma el **corte de energía** de un circuito simulado y activa un **extractor de
ventilación** mediante un módulo relay. El estado se publica por **MQTT** (broker
Mosquitto local) con **QoS diferenciado** según la criticidad y mecanismo **LWT**
para detectar caídas del nodo. Los datos se visualizan en **Node-RED** y **Grafana**.

> Proyecto del curso de IoT — Universidad Nacional Mayor de San Marcos.

## Arquitectura

```
┌────────────────────────────┐     WiFi 2.4GHz    ┌────────────────────────────────────┐
│  ESP32-S3 (C++/PlatformIO)  │   MQTT/TCP :1883   │  LAPTOP (Docker)                   │
│  MQ-2, Relay, OLED, Buzzer  │  ───────────────►  │  Mosquitto → Node-RED → InfluxDB   │
│  LED RGB (semáforo)         │                    │                     → Grafana      │
└────────────────────────────┘                    └────────────────────────────────────┘
```

## Estructura del repo

```
proyecto-gas-glp/
├── firmware/                  Firmware del ESP32-S3 (PlatformIO, C++)
│   ├── platformio.ini
│   ├── include/
│   │   ├── config.h           pines, calibración, umbrales, topics
│   │   ├── secrets.example.h  PLANTILLA (se sube)
│   │   └── secrets.h          datos privados (NO se sube — .gitignore)
│   └── src/main.cpp
├── server/                    Stack de la laptop (Docker)
│   ├── docker-compose.yml
│   ├── mosquitto/config/mosquitto.conf
│   └── README.md              setup de Node-RED, InfluxDB y Grafana
├── docs/
│   └── CONEXIONES.md          diagrama de pines y montaje físico
└── README.md
```

## Puesta en marcha

### A) Firmware (VS Code + PlatformIO)

1. Instala **VS Code** y la extensión **PlatformIO IDE**.
2. Copia los secretos y llénalos con tus datos:
   ```bash
   cd firmware/include
   cp secrets.example.h secrets.h      # Windows: copy secrets.example.h secrets.h
   ```
   Edita `secrets.h`: SSID/clave del hotspot (**2.4 GHz**), nombre de la laptop
   para mDNS e IP de respaldo.
3. Abre la carpeta `firmware/` en VS Code → **Build** (instala las librerías solas)
   → conecta el ESP32-S3 por USB-C → **Upload** → **Monitor**.

### B) Servidor (Docker en la laptop)

```bash
cd server
docker compose up -d
```
Sigue [`server/README.md`](server/README.md) para configurar Node-RED, InfluxDB y Grafana.

### C) Montaje físico

Sigue el pinout y el **divisor de tensión del MQ-2** en
[`docs/CONEXIONES.md`](docs/CONEXIONES.md). **Deja el MQ-2 en burn-in ~24 h** la
primera vez.

## Red (importante)

- Usa el **hotspot del celular en 2.4 GHz** (el ESP32 no ve 5 GHz).
  iPhone → activa *"Maximizar compatibilidad"*.
- El ESP32 encuentra la laptop por **mDNS** (`<nombre>.local`) con **IP de respaldo**
  si el multicast está bloqueado.
- Abre el **puerto 1883** en el firewall de la laptop.

## Topics MQTT y QoS

| Topic                    | QoS | Retained | Contenido                          |
|--------------------------|-----|----------|------------------------------------|
| `gas/nodo1/telemetria`   | 0   | no       | ppm periódicos                     |
| `gas/nodo1/alarma`       | 1   | sí       | evento crítico de fuga             |
| `gas/nodo1/estado`       | 1   | sí       | `online` / `offline` (LWT)         |
| `gas/nodo1/actuador`     | 1   | no       | estado de relays (corte/extractor) |

## Advertencias de seguridad

- Para pruebas de fuga usa gas de encendedor **SIN LLAMA**, en área ventilada.
- El MQ-2 detecta GLP, humo, propano, metano; no es específico de un solo gas.
- Es un prototipo académico, **no un dispositivo de seguridad certificado**.
