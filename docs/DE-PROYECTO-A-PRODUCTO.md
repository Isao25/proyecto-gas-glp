# De proyecto a producto — Guía de sustentación

Documento para preparar la **feria tecnológica** y la **sustentación**: qué hace cada
pieza, cómo presentarlo, cómo diferenciarlo de "un simple proyecto de curso", el valor
para un usuario real, y la hoja de ruta para elevarlo a un **producto sostenible**.

---

## 1. ¿Para qué sirve cada contenedor?

El servidor levanta **4 servicios** en Docker. Cada uno es una **capa distinta** de la
arquitectura IoT (sensor → transporte → procesamiento → almacenamiento → visualización).

| Contenedor | Puerto | Qué hace | Por qué es necesario |
|---|---|---|---|
| **Mosquitto** | 1883 | Broker MQTT: recibe y reparte los mensajes del ESP32 | Es el "cartero". Sin él, el nodo no puede comunicar nada |
| **Node-RED** | 1880 | Puente: se suscribe a MQTT y **escribe en InfluxDB**. También reglas y alertas | Conecta el MQTT con la base de datos (Mosquitto no habla con InfluxDB directo) |
| **InfluxDB** | 8086 | Base de datos de **series de tiempo**: guarda cada lectura con su hora | La memoria histórica del sistema (ppm, alarmas) |
| **Grafana** | 3000 | **Visualización** profesional: gráficas, medidores, alertas | La cara para monitorear y presentar |

**Flujo completo:**
```
ESP32 → Mosquitto → Node-RED → InfluxDB → Grafana
        transporte   puente      memoria    visualización
```

**¿Qué es redundante?** Tienes DOS visualizadores (el dashboard de Node-RED y Grafana).
No es un error: se justifican como **complementarios**:
- **Node-RED `/ui`** = monitoreo operativo **en vivo** (el momento).
- **Grafana** = análisis **histórico** y presentación formal (la evolución).

---

## 2. Cómo explicarlo en una feria tecnológica (el pitch)

### Pitch de 30 segundos (el gancho)
> "En 2020, una fuga de gas en Villa El Salvador causó 34 muertes. El GLP es el 45% del
> consumo doméstico en Perú, y casi ningún hogar tiene detección automática. Nosotros
> construimos un nodo IoT que **detecta la fuga, corta la energía solo y avisa en tiempo
> real** — sin depender de que alguien esté presente."

### El demo en vivo (lo que más impacta)
1. Muestra el semáforo en **verde** (normal).
2. Acerca gas al sensor → sube a **ámbar** (pre-alarma, ventila).
3. Más gas → **rojo + buzzer + CLIC del relé** (corte automático).
4. Señala el **dashboard** actualizándose en tiempo real.
5. Retira el gas → vuelve a verde solo (histéresis).

### Qué recalcar mientras demuestras
- "El **corte es autónomo**: ocurre en el ESP32 en milisegundos, **aunque se caiga el WiFi**."
- "Todo se **registra y grafica** — no es solo un LED que prende, es un sistema con memoria."
- "Usa **MQTT**, el protocolo estándar de la industria IoT, con **QoS y LWT** (detecta si el nodo se cae)."

---

## 3. Cómo diferenciarlo de "un simple proyecto"

Un proyecto de curso típico es "sensor → LED". El tuyo tiene ingeniería de **producto**:

| Un simple proyecto | Tu proyecto (nivel producto) |
|---|---|
| Sensor prende un LED | **Máquina de estados** con histéresis + **corte automático** físico |
| Todo en un solo Arduino | **Arquitectura por capas** (nodo + servidor + dashboard) |
| Sin comunicación | **MQTT con QoS diferenciado** (0 telemetría / 1 alarma) |
| No sabe si el equipo falla | **LWT**: el sistema detecta si el nodo se desconecta |
| Datos que se pierden | **Persistencia** en base de datos + histórico |
| "En mi máquina funciona" | **Reproducible** con Docker (Mac/Windows) y versionado en Git |
| Un dispositivo | **Base escalable** a múltiples nodos y a la nube |

**Frase para sustentar:** *"No construimos solo un detector; construimos la arquitectura
de comunicación y datos que permite que esto escale a un producto real y multi-dispositivo."*

---

## 4. Valor para un usuario real

### El problema
- El GLP representa ~45% del consumo doméstico de energía en Perú.
- Accidentes por fuga (Villa El Salvador 2020: 34 fallecidos) evidencian la falta de
  detección automática accesible.
- Los detectores comerciales certificados son caros o solo hacen ruido local (nadie se
  entera si no está en casa).

### A quién le sirve
- **Hogares** con cocina a gas (balón de GLP).
- **Restaurantes / locales** con cocinas industriales (alto riesgo).
- **Distribuidoras de gas** (monitoreo de clientes, valor agregado).

### Qué gana el usuario
- **Seguridad activa**: corta y ventila solo, sin depender de que alguien reaccione.
- **Aviso remoto**: se entera **aunque no esté en casa** (ver sección 5).
- **Tranquilidad + evidencia**: histórico de eventos (útil para seguros, mantenimiento).

---

## 5. Alertas al celular — ¿se puede? SÍ

Es totalmente viable y es **el salto más grande hacia "producto"**. Opciones (de más
fácil a más robusta):

| Método | Cómo | Dificultad |
|---|---|---|
| **Telegram** | Un bot de Telegram desde Node-RED (`node-red-contrib-telegrambot`). Gratis, mensaje instantáneo al celular | 🟢 Fácil (ideal para la feria) |
| **Correo** | Node-RED envía email en cada alarma | 🟢 Fácil |
| **Push / Pushover** | Notificación push a una app en el celular | 🟡 Media |
| **SMS** | Vía Twilio (SMS real, funciona sin internet en el celular) | 🟡 Media (de pago) |
| **App propia** | App móvil suscrita al MQTT (muestra estado en vivo) | 🔴 Avanzada |

**Recomendado para demostrar en la feria:** un **bot de Telegram**. Cuando el sensor
entra en ALARMA, Node-RED envía *"⚠️ Fuga de gas detectada — corte activado"* al celular
en 1 segundo. Es gratis, rápido de montar y **impacta muchísimo en la demo**.

> Esto convierte el sistema de "alarma local" a "alarma que te alcanza **donde estés**" —
> justo lo que diferencia un producto de un prototipo.

---

## 6. ¿Dónde se monitorea? (local vs. remoto)

### Ahora (local)
- Todo corre en **la laptop** (Mosquitto, Node-RED, InfluxDB, Grafana).
- Se monitorea en el **navegador de la misma red** (dashboards en `localhost`).
- Limitación: **solo funciona en la red local** (casa/hotspot). Fuera de casa no ves nada.

### Para monitoreo remoto (el paso a producto)
Para verlo **desde cualquier lugar**, se lleva a la nube:

| Opción | Qué da |
|---|---|
| **Broker MQTT en la nube** (HiveMQ, EMQX Cloud) | El nodo publica a internet, no solo a la LAN |
| **InfluxDB Cloud + Grafana Cloud** | Dashboards accesibles desde cualquier navegador |
| **Raspberry Pi como gateway 24/7** | Servidor siempre encendido en casa (no la laptop) |
| **Túnel / VPN** | Exponer el dashboard local de forma segura |

**Camino natural:** hoy la laptop es el "servidor de demostración"; en producto, el
servidor vive en la **nube** (o en un gateway 24/7) y el usuario monitorea desde una
**app o web** más las **alertas al celular**.

---

## 7. Hoja de ruta: de prototipo a producto sostenible

### Lo que YA tienes (funcional) ✅
- Detección + corte automático + señalización (LED/buzzer/relé).
- MQTT con QoS + LWT, persistencia y dashboards.
- Arquitectura por capas, reproducible (Docker + Git).

### Lo que lo eleva a producto 🚀

**Corto plazo (alto impacto, bajo costo):**
1. **Alertas al celular** (Telegram) — el mayor salto de valor.
2. **Servidor 24/7** (Raspberry Pi o nube) en vez de la laptop.
3. **Respaldo de energía** (batería/UPS) — un detector de seguridad no puede apagarse con la luz.

**Mediano plazo:**
4. **Monitoreo remoto en la nube** (accesible desde cualquier lugar).
5. **Multi-nodo**: varios sensores (cocina, balón, exterior) y varias casas.
6. **Robustez**: watchdog, reconexión automática, auto-diagnóstico del sensor.

**Largo plazo (visión de producto):**
7. **Certificación / normativa** de seguridad (lo que exige un producto real).
8. **Modelo de negocio**: venta del equipo + **suscripción de monitoreo** (SaaS).
   Clientes: hogares, restaurantes, distribuidoras de GLP.
9. **Analítica**: tendencias, mantenimiento predictivo del sensor, reportes.

### La narrativa de cierre (para la sustentación)
> "Tenemos un **prototipo funcional** que ya hace lo esencial: detecta y corta solo. La
> arquitectura que elegimos —MQTT, capas, persistencia— **no es la de un experimento,
> es la de un producto escalable**. Con alertas al celular y monitoreo en la nube, esto
> pasa de 'proyecto de curso' a un **servicio de seguridad accesible** que puede salvar
> vidas en los hogares que hoy no tienen ninguna protección."

---

## Resumen de una línea

**Ya tienes un sistema funcional con arquitectura de producto.** Los 3 pasos que más lo
elevan: **(1) alertas al celular**, **(2) servidor 24/7 / nube**, **(3) respaldo de
energía**. Con eso dejas de tener un proyecto y tienes un **producto de seguridad
sostenible**.
