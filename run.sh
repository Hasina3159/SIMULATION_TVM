#!/usr/bin/env bash
# Lance toute la demo en une commande : websocket Mosquitto (si besoin),
# build, collector, tvm-simulator, frontend Vite. Ctrl+C arrete tout proprement.
set -e

cd "$(dirname "$0")"

if ! ss -tln 2>/dev/null | grep -q ':9001'; then
    echo "Listener websocket Mosquitto absent -> application de mosquitto/websockets.conf (sudo requis)"
    sudo cp mosquitto/websockets.conf /etc/mosquitto/conf.d/websockets.conf
    sudo systemctl restart mosquitto
    sleep 1
fi

echo "=== Build ==="
cmake -B build >/dev/null
cmake --build build -j"$(nproc)"

rm -f collector.db

echo "=== Lancement collector + tvm-simulator ==="
./build/collector > /tmp/tvm_collector.log 2>&1 &
COLLECTOR_PID=$!
sleep 1
./build/tvm-simulator > /tmp/tvm_simulator.log 2>&1 &
TVM_PID=$!
sleep 1

cleanup() {
    echo
    echo "=== Arret ==="
    kill "$COLLECTOR_PID" "$TVM_PID" 2>/dev/null || true
    wait "$COLLECTOR_PID" "$TVM_PID" 2>/dev/null || true
}
trap cleanup EXIT

echo "Collector (logs: /tmp/tvm_collector.log) : pid $COLLECTOR_PID"
echo "tvm-simulator (logs: /tmp/tvm_simulator.log) : pid $TVM_PID"

if [ ! -d client-app/node_modules ]; then
    echo "=== npm install (client-app) ==="
    (cd client-app && npm install)
fi

echo "=== Frontend : http://localhost:5173 (Ctrl+C pour tout arreter) ==="
(cd client-app && npm run dev)
