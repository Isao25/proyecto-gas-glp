# Cómo levantar todo el sistema 🚀

Guía para poner en marcha el proyecto completo: el **servidor** (Mosquitto, Node-RED,
InfluxDB, Grafana) y el **firmware** del ESP32. Hay dos caminos según la situación.

---

## Requisitos (una sola vez)

- **Docker Desktop** instalado y **corriendo** (en Windows necesita WSL2).
- **VS Code** + extensión **PlatformIO** (para el firmware).
- Estar dentro de la carpeta del proyecto:
  ```bash
  cd proyecto-gas-glp
  ```

---

## CAMINO A — Volver a levantar en ESTA laptop (lo normal)

Como los datos y la configuración quedan guardados en el disco, basta con:

```bash
cd server
docker compose up -d
```

Espera ~20 s y abre los enlaces (ver más abajo). Todo (InfluxDB, el flujo de
Node-RED, el dashboard de Grafana) sigue configurado de antes.

> Si en Grafana no ves la gráfica al abrir, recarga la página con **Cmd/Ctrl + Shift + R**
> (el auto-refresh no recarga el diseño del panel, solo los datos).

Para apagar todo:
```bash
docker compose down
```

---

## CAMINO B — Primera vez o en una máquina nueva (ej. tu compañero)

En una laptop nueva no existe la configuración todavía. Un solo comando la crea toda:

```bash
./scripts/setup-servidor.sh
```

Ese script automáticamente:
1. Levanta los 4 contenedores.
2. Configura **InfluxDB** (organización `unmsm`, bucket `gas`, token).
3. Instala las paletas de **Node-RED**, carga el flujo y le pone el token.
4. Crea en **Grafana** la fuente de datos y el dashboard.

Al terminar te imprime los enlaces. (Tarda un par de minutos la primera vez porque
Node-RED baja las paletas.)

> **Windows:** ejecuta el script desde **Git Bash** o **WSL** (no en CMD/PowerShell).

---

## Enlaces del sistema

| Servicio | Enlace | Acceso |
|---|---|---|
| **Grafana** (dashboard principal) | http://localhost:3000/d/glp-dash | `admin` / `admin` |
| **Node-RED** (flujo visual) | http://localhost:1880 | — |
| **Node-RED** (dashboard simple) | http://localhost:1880/ui | — |
| **InfluxDB** (datos crudos) | http://localhost:8086 | `admin` / `admin12345` |
| **Mosquitto** (broker MQTT) | `localhost:1883` | (puerto, no web) |

---

## Simular datos (sin la placa física)

Con el servidor arriba, puedes imitar al ESP32 publicando lecturas:

```bash
./scripts/simular-esp32.sh          # un ciclo de fuga (sube y baja)
./scripts/simular-esp32.sh loop     # continuo (Ctrl+C para parar)
```

Verás la curva moverse en Grafana y en el dashboard de Node-RED.

Prueba manual rápida (publicar un mensaje suelto):
```bash
docker exec mosquitto mosquitto_pub -t gas/nodo1/telemetria -m '{"ppm":1500}'
```

---

## Firmware del ESP32 (cuando tengas la placa)

1. Abre la carpeta **`firmware/`** en VS Code (PlatformIO).
2. Crea tus secretos:
   ```bash
   cd firmware/include
   cp secrets.example.h secrets.h     # Windows: copy secrets.example.h secrets.h
   ```
   Edita `secrets.h`: SSID/clave del hotspot (**2.4 GHz**), nombre de la laptop e IP de respaldo.
3. Conecta el ESP32 por USB-C → **Upload and Monitor** en PlatformIO.

> El ESP32 y la laptop deben estar en la **misma red WiFi (2.4 GHz)** y el **puerto 1883**
> abierto en el firewall de la laptop.

---

## Solución de problemas

| Problema | Solución |
|---|---|
| Grafana no muestra la gráfica | Recarga la página (**Cmd/Ctrl + Shift + R**) y revisa el rango "Last 15 minutes" |
| "Cannot connect to Docker" | Abre Docker Desktop y espera a que arranque |
| Node-RED: nodos desconocidos | Faltan las paletas → corre `./scripts/setup-servidor.sh` |
| El ESP32 no conecta | Hotspot en 2.4 GHz + puerto 1883 en el firewall + misma red |
| Ver el estado de los contenedores | `cd server && docker compose ps` |
