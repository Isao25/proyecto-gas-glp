#!/usr/bin/env bash
# =============================================================================
#  setup-servidor.sh  -  Configura el stack COMPLETO la PRIMERA VEZ
#  (InfluxDB + Node-RED + Grafana) de forma automatica.
#
#  Uso:   ./scripts/setup-servidor.sh
#
#  Es idempotente: si ya esta configurado, no rompe nada.
#  Requiere Docker Desktop corriendo.
# =============================================================================
set -e
cd "$(dirname "$0")/.."          # raiz del repo

# ---- Parametros (cambialos si quieres) -------------------------------------
INFLUX_ORG="unmsm"
INFLUX_BUCKET="gas"
INFLUX_USER="admin"
INFLUX_PASS="admin12345"
INFLUX_TOKEN="glp-token-supersecreto-123456"

echo "==> 1/5  Levantando contenedores..."
( cd server && docker compose up -d )

echo "==> 2/5  Configurando InfluxDB (org=$INFLUX_ORG, bucket=$INFLUX_BUCKET)..."
until docker exec influxdb influx ping >/dev/null 2>&1; do sleep 1; done
docker exec influxdb influx setup --force \
  --org "$INFLUX_ORG" --bucket "$INFLUX_BUCKET" \
  --username "$INFLUX_USER" --password "$INFLUX_PASS" \
  --token "$INFLUX_TOKEN" --retention 0 >/dev/null 2>&1 \
  && echo "    InfluxDB configurado." \
  || echo "    InfluxDB ya estaba configurado (ok)."

echo "==> 3/5  Node-RED: paletas + flujo + token..."
# Espera a que Node-RED cree su settings.js
until [ -f server/nodered/settings.js ]; do sleep 1; done
# Paletas (dashboard + InfluxDB)
docker exec -w /data nodered npm install node-red-dashboard node-red-contrib-influxdb --no-audit --no-fund >/dev/null 2>&1
# Guarda credenciales en texto plano (desactiva el cifrado)
docker exec nodered sed -i 's|//credentialSecret: "a-secret-key",|credentialSecret: false,|' /data/settings.js || true
# Carga el flujo y el token de InfluxDB
cp server/nodered/flow-glp.json server/nodered/flows.json
cat > server/nodered/flows_cred.json <<EOF
{ "idb-conf": { "token": "$INFLUX_TOKEN" } }
EOF
docker restart nodered >/dev/null
until docker logs nodered 2>&1 | grep -q "Started flows"; do sleep 1; done
echo "    Node-RED con el flujo cargado."

echo "==> 4/5  Grafana: fuente de datos + dashboard..."
until curl -s -o /dev/null -w "%{http_code}" http://localhost:3000/api/health 2>/dev/null | grep -q 200; do sleep 1; done
# Fuente de datos InfluxDB (ignora error si ya existe)
curl -s -u admin:admin -X POST http://localhost:3000/api/datasources \
  -H "Content-Type: application/json" \
  -d "{
    \"name\": \"InfluxDB-GLP\", \"type\": \"influxdb\", \"uid\": \"influxdb-glp\",
    \"access\": \"proxy\", \"url\": \"http://influxdb:8086\",
    \"jsonData\": { \"version\": \"Flux\", \"organization\": \"$INFLUX_ORG\", \"defaultBucket\": \"$INFLUX_BUCKET\", \"httpMode\": \"POST\" },
    \"secureJsonData\": { \"token\": \"$INFLUX_TOKEN\" }
  }" >/dev/null 2>&1 || true
# Dashboard
curl -s -u admin:admin -X POST http://localhost:3000/api/dashboards/db \
  -H "Content-Type: application/json" \
  -d @server/grafana-provision/dashboard-glp.json >/dev/null 2>&1
echo "    Grafana provisionado."

echo "==> 5/5  Listo!"
echo ""
echo "  Node-RED   -> http://localhost:1880        (flujo) "
echo "  Node-RED UI-> http://localhost:1880/ui     (dashboard simple)"
echo "  InfluxDB   -> http://localhost:8086        ($INFLUX_USER / $INFLUX_PASS)"
echo "  Grafana    -> http://localhost:3000/d/glp-dash   (admin / admin)"
echo ""
echo "  Para simular datos:  ./scripts/simular-esp32.sh loop"
