# SIMULATION_TVM

Simulateur d'un TVM (distributeur de titres de transport) connecté en MQTT, avec paiement
espèces (caisse locale) et carte (débit via un `collector` SQLite). Périmètre : un seul TVM,
pensé comme démo technique (voir [CDC_TVM_Solo_Entretien.md](CDC_TVM_Solo_Entretien.md) pour le
détail du périmètre, et [CDC_Simulateur_TVM_MQTT.md](CDC_Simulateur_TVM_MQTT.md) pour l'ambition
complète à plus long terme).

## Prérequis

- Mosquitto (`sudo apt install mosquitto mosquitto-clients`), déjà actif en service système sur
  `tcp://localhost:1883`
- Paho MQTT C++ (`libpaho-mqttpp-dev`, `libpaho-mqtt-dev`), SQLite3 (`libsqlite3-dev`)
- CMake ≥ 3.16, Node.js ≥ 18 (pour le frontend)

## Activer le websocket MQTT (requis pour le frontend)

Le broker système n'écoute par défaut qu'en TCP brut (1883) — le navigateur a besoin d'un
listener websocket pour s'y connecter :

```bash
sudo cp mosquitto/websockets.conf /etc/mosquitto/conf.d/websockets.conf
sudo systemctl restart mosquitto
```

## Build et lancement du backend

```bash
cmake -B build
cmake --build build -j"$(nproc)"

./build/collector        # terminal 1 — seed le compte cl_demo (50€) si absent
./build/tvm-simulator     # terminal 2 — TVM "tvm_01", attend les commandes MQTT
```

## Lancer le frontend

```bash
cd client-app
npm install
npm run dev               # http://localhost:5173
```

## Tests

```bash
cmake --build build --target test_supervisor test_card_reader test_comptes
./build/test_supervisor    # TvmSupervisor + calculer_rendu
./build/test_card_reader   # CardReader (fake publisher, sans broker)
./build/test_comptes       # ComptesManager (SQLite en mémoire)
```

## Vérifier manuellement sans le frontend

```bash
mosquitto_sub -h localhost -t 'tvm/tvm_01/#' -v &

mosquitto_pub -h localhost -t tvm/tvm_01/commands/selection_titre \
  -m '{"type_titre":"ticket_unique","quantite":1}'
mosquitto_pub -h localhost -t tvm/tvm_01/commands/inserer_espece -m '{"denomination":200}'
# ou, pour un paiement carte :
mosquitto_pub -h localhost -t tvm/tvm_01/commands/payer_carte -m '{"compte_id":"cl_demo"}'
```

## Ce qui est hors périmètre (volontairement)

Authentification, flotte multi-TVM, réservation, dashboard admin, API REST — voir la section 1.1
de `CDC_TVM_Solo_Entretien.md` pour le détail et le raisonnement.
