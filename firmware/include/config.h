#pragma once
// =============================================================================
//  CONFIGURACION DEL NODO  (pines, calibracion, umbrales, topics MQTT)
//  Proyecto: Detector de Fuga de GLP con corte automatico  -  ESP32-S3
//  Aqui NO van contraseñas ni IPs -> eso va en secrets.h
// =============================================================================

// ---------------------------------------------------------------------------
//  PINES  (GPIO seguros del ESP32-S3 DevKitC-1)
//  Evitar: 19/20 (USB nativo), 0/45/46 (arranque), 26-37 (flash/PSRAM)
// ---------------------------------------------------------------------------
#define PIN_MQ2_AO            4    // Salida analogica del MQ-2 -> ADC1 (VIA DIVISOR /2)
#define PIN_OLED_SDA          8    // I2C datos
#define PIN_OLED_SCL          9    // I2C reloj
#define PIN_RELAY_CORTE       10   // Relay CH1 -> corta la energia del circuito simulado
#define PIN_RELAY_EXTRACTOR   11   // Relay CH2 -> enciende el extractor / motor DC
#define PIN_BUZZER            12   // Buzzer (activo: HIGH = suena)
#define PIN_LED_R             13   // LED RGB rojo   (470 ohm)
#define PIN_LED_G             14   // LED RGB verde  (470 ohm)
#define PIN_LED_B             21   // LED RGB azul   (470 ohm)

// La mayoria de modulos relay de 4ch son ACTIVOS EN BAJO (se activan con LOW).
// Si el tuyo se activa con HIGH, cambia esto a false.
#define RELAY_ACTIVO_EN_BAJO  true

// LED RGB de catodo comun (HIGH enciende). Si es anodo comun, pon false.
#define LED_ANODO_COMUN       false

// ---------------------------------------------------------------------------
//  MQ-2  /  CALIBRACION  (metodo Rs/Ro con curva de GLP del datasheet)
// ---------------------------------------------------------------------------
// El ADC del ESP32 mide 0-3.3 V, pero pusimos un DIVISOR /2 (2x 1K) para
// proteger el pin. Por eso declaramos 6.6 V de fondo de escala: asi la libreria
// reconstruye el voltaje REAL de la salida del MQ-2 (ADC 3.3 V = 6.6 V reales).
#define MQ2_VOLTAGE_RESOLUTION   6.6f
#define MQ2_ADC_BITS             12       // ESP32: ADC de 12 bits (0-4095)
#define MQ2_RATIO_CLEAN_AIR      9.83f    // Rs/Ro tipico del MQ-2 en aire limpio
#define MQ2_LPG_A                574.25f  // Coeficiente 'a' curva GLP (PPM = a*(Rs/Ro)^b)
#define MQ2_LPG_B                -2.222f  // Coeficiente 'b' curva GLP

// ---------------------------------------------------------------------------
//  UMBRALES DE GLP EN PPM  (con HISTERESIS para que no oscile en el borde)
//  LEL del GLP ~ 18000 ppm. Aqui trabajamos muy por debajo, en % del LEL.
//
//  NOTA: umbrales de DEMO, ajustados al gas de prueba disponible (encendedor /
//  alcohol dan lecturas modestas ~10-50 ppm con exposicion breve). Para el
//  proyecto final con fuente de gas real y sostenida, usar valores realistas
//  (ej. PRE-ALARMA 1000 / ALARMA 2000, ~% del LEL).
// ---------------------------------------------------------------------------
#define PPM_PREALARMA_ON      30   // sube a PRE-ALARMA
#define PPM_PREALARMA_OFF     18   // baja de PRE-ALARMA a NORMAL
#define PPM_ALARMA_ON         40   // sube a ALARMA -> corte automatico
#define PPM_ALARMA_OFF        25   // baja de ALARMA a PRE-ALARMA

// ---------------------------------------------------------------------------
//  TIEMPOS
// ---------------------------------------------------------------------------
#define INTERVALO_LECTURA_MS      1000   // cada cuanto se lee el sensor
#define INTERVALO_TELEMETRIA_MS   3000   // cada cuanto se publica por MQTT
#define CALENTAMIENTO_MS          20000  // warm-up corto en cada arranque (demo)

// ---------------------------------------------------------------------------
//  OLED
// ---------------------------------------------------------------------------
#define OLED_ADDR   0x3C
#define OLED_ANCHO  128
#define OLED_ALTO   64

// ---------------------------------------------------------------------------
//  TOPICS MQTT  y  QoS
//  QoS 0 -> telemetria (si se pierde un dato, no pasa nada)
//  QoS 1 -> alarma y estado (deben llegar SI o SI)
// ---------------------------------------------------------------------------
#define TOPIC_TELEMETRIA   "gas/nodo1/telemetria"   // ppm periodicos      (QoS 0)
#define TOPIC_ALARMA       "gas/nodo1/alarma"       // evento critico      (QoS 1, retained)
#define TOPIC_ESTADO       "gas/nodo1/estado"       // online/offline LWT  (QoS 1, retained)
#define TOPIC_ACTUADOR     "gas/nodo1/actuador"     // estado de relays    (QoS 1)
