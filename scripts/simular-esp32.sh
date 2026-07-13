#!/usr/bin/env bash
# Simula al ESP32 publicando telemetria por MQTT hacia Mosquitto.
# Uso:  ./simular-esp32.sh          -> un ciclo de fuga (sube y baja)
#       ./simular-esp32.sh loop     -> continuo (Ctrl+C para parar)
set -e
pub() { docker exec mosquitto mosquitto_pub -t "$1" -q "$2" -m "$3"; }

ciclo() {
  pub gas/nodo1/estado 1 online
  for v in 220 380 560 820 1100 1500 1900 2300 2500 2100 1600 1150 780 500 320 240; do
    estado="NORMAL"; [ "$v" -ge 1000 ] && estado="PRE-ALARMA"; [ "$v" -ge 2000 ] && estado="ALARMA"
    pub gas/nodo1/telemetria 0 "{\"ppm\":$v,\"estado\":\"$estado\",\"rssi\":-60}"
    [ "$v" -ge 2000 ] && pub gas/nodo1/alarma 1 "{\"alarma\":true,\"ppm\":$v}"
    echo "ppm=$v ($estado)"; sleep 1
  done
  pub gas/nodo1/alarma 1 "{\"alarma\":false,\"ppm\":240}"
}

if [ "$1" = "loop" ]; then
  echo "Simulacion continua. Ctrl+C para parar."
  while true; do ciclo; done
else
  ciclo
fi
