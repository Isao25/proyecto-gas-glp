# Diagrama de conexiones — Nodo detector de GLP (ESP32-S3)

## Tabla de pines

| Módulo               | Pin del módulo | GPIO ESP32-S3 | Nota                              |
|----------------------|----------------|---------------|-----------------------------------|
| MQ-2                 | AOUT           | **GPIO4** (ADC1) | ⚠️ **vía divisor de tensión ÷2** |
| MQ-2                 | VCC / GND      | **5 V** / GND  | el heater necesita 5 V            |
| OLED SSD1306         | SDA            | GPIO8          | I2C (dirección 0x3C)              |
| OLED SSD1306         | SCL            | GPIO9          | I2C                               |
| OLED SSD1306         | VCC / GND      | 3V3 / GND      |                                   |
| Relay CH1 (corte)    | IN1            | GPIO10         | corta el circuito simulado        |
| Relay CH2 (extractor)| IN2            | GPIO11         | enciende ventilador / motor DC    |
| Relay                | VCC / GND      | 5 V / GND      | activo en BAJO (típico)           |
| Buzzer               | +              | GPIO12         | activo (HIGH = suena)             |
| LED RGB — R          | R              | GPIO13         | resistencia 470 Ω                 |
| LED RGB — G          | G              | GPIO14         | resistencia 470 Ω                 |
| LED RGB — B          | B              | GPIO21         | resistencia 470 Ω                 |

> **No usar**: GPIO19/20 (USB nativo), GPIO0/45/46 (arranque), GPIO26–37 (flash/PSRAM).

## ⚠️ Divisor de tensión del MQ-2 (obligatorio)

La salida AOUT del MQ-2 llega hasta ~5 V y el ADC del ESP32 aguanta **máximo 3.3 V**.
Con dos resistencias de **1 kΩ** se hace un divisor **÷2** (5 V → 2.5 V, seguro):

```
MQ-2 AOUT ──[ R1 = 1KΩ ]──┬──► GPIO4 (ADC1)
                          │
                       [ R2 = 1KΩ ]
                          │
                         GND
```

En el firmware esto se compensa declarando `MQ2_VOLTAGE_RESOLUTION = 6.6` en `config.h`.

## Alimentación (regla de oro)

```
Fuente 5V externa ──► MQ-2 (VCC) + Relay (VCC)
USB-C             ──► ESP32-S3
TODAS las tierras (GND) UNIDAS entre sí   ← imprescindible
```

No alimentar el heater del MQ-2 ni la bobina del relay desde el pin 3V3 del ESP32.

## Orden de montaje (probar módulo por módulo)

1. ESP32 solo → "blink".
2. OLED → texto de prueba (I2C 0x3C).
3. LED RGB + buzzer.
4. Relay → clic de los 2 canales.
5. MQ-2 con divisor → verificar con multímetro que GPIO4 nunca pasa de ~2.5 V.
6. Motor DC / extractor al Relay CH2.

## Máquina de estados (resumen)

| Estado      | Color LED | Corte (CH1) | Extractor (CH2) | Buzzer |
|-------------|-----------|-------------|-----------------|--------|
| NORMAL      | Verde     | No          | Off             | Off    |
| PRE-ALARMA  | Ámbar     | No          | On (preventivo) | Off    |
| ALARMA      | Rojo      | **Sí**      | On              | On     |

Umbrales (ppm, ajustables en `config.h` tras calibrar):
PRE-ALARMA on/off = 1000/800 · ALARMA on/off = 2000/1500 (histéresis).
