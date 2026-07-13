# Servidor (laptop) — Mosquitto + Node-RED + InfluxDB + Grafana

Todo corre en Docker. Requisito: **Docker Desktop** instalado (en Windows necesita WSL2).

## 1. Levantar el stack

```bash
cd server
docker compose up -d
```

Verifica que los 4 contenedores estén arriba:

```bash
docker compose ps
```

Accesos desde el navegador de la laptop:
- Node-RED → http://localhost:1880
- InfluxDB → http://localhost:8086
- Grafana  → http://localhost:3000  (usuario/clave inicial: `admin` / `admin`)

## 2. Probar el broker (antes de tocar el ESP32)

En una terminal, suscríbete a todo:

```bash
docker exec -it mosquitto mosquitto_sub -t 'gas/#' -v
```

En otra terminal, publica un mensaje de prueba:

```bash
docker exec -it mosquitto mosquitto_pub -t 'gas/nodo1/telemetria' -m '{"ppm":123}'
```

Si lo ves en la primera terminal, el broker funciona.

> Para que el **ESP32** conecte desde el hotspot, abre el **puerto 1883** en el firewall
> de la laptop y confirma que ambos están en la **misma red (2.4 GHz)**.

## 3. InfluxDB (primera vez)

1. Abre http://localhost:8086 → crea usuario, **organización** (ej. `unmsm`) y
   **bucket** (ej. `gas`).
2. Copia el **API Token** que te genera (lo usará Node-RED y Grafana).

## 4. Node-RED — puente MQTT → InfluxDB

En http://localhost:1880 arma este flujo (menú → *Manage palette* →
instala `node-red-contrib-influxdb` si no está):

```
[mqtt in: gas/nodo1/telemetria]──►[json]──►[function: dar formato]──►[influxdb out]
[mqtt in: gas/nodo1/alarma]─────►[json]──►[debug / influxdb out]
[mqtt in: gas/nodo1/estado]─────►[debug]   (online/offline por LWT)
```

- **mqtt in**: server = `mosquitto` (¡nombre del contenedor, no localhost!), puerto `1883`.
  Topic `gas/nodo1/telemetria`, QoS 0. Repite para `alarma` (QoS 1) y `estado` (QoS 1).
- **influxdb out**: host = `influxdb`, puerto `8086`, con tu Organización, Bucket y Token.
- Exporta el flujo terminado a `nodered/flows.json` para versionarlo en el repo
  (menú → Export → clipboard → pégalo en el archivo).

## 5. Grafana — dashboard

1. http://localhost:3000 → *Connections → Data sources → Add → InfluxDB*.
   - Query language: **Flux**
   - URL: `http://influxdb:8086`
   - Organization, Token y Default bucket (los de InfluxDB).
2. Crea un dashboard con paneles:
   - **Serie temporal**: ppm de GLP en el tiempo.
   - **Stat / Gauge**: último valor de ppm.
   - **State timeline**: estado online/offline (LWT).
   - **Anotaciones**: eventos de alarma.
3. Exporta el dashboard (JSON) a `grafana/dashboards/` para versionarlo.

## Apagar / reiniciar

```bash
docker compose down       # apaga (los datos persisten en las carpetas locales)
docker compose restart    # reinicia
```
