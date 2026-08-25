import mqtt from 'mqtt'
import { reactive } from 'vue'

// Connecte au broker sur le port websocket (voir mosquitto/websockets.conf)
// et maintient un etat reactif reflete depuis les topics retained/publies
// par le TVM. Un seul point d'entree MQTT pour toute l'app.
export function useTvmConnection(tvmId, brokerUrl = 'ws://localhost:9001') {
  const state = reactive({
    connected: false,
    etat: 'IDLE',
    caisse: null,
    derniereVente: null,
    derniereErreur: null,
    derniereAnnulation: null,
    dernierType: null,       // 'vente' | 'erreur' | 'annulation' : le plus recent des trois
    dernierEvenementId: 0,   // incremente a chaque vente/erreur/annulation, pour detecter la fraicheur
    journal: [],
  })

  const client = mqtt.connect(brokerUrl, { reconnectPeriod: 2000 })

  client.on('connect', () => {
    state.connected = true
    client.subscribe(`tvm/${tvmId}/#`)
  })
  client.on('reconnect', () => { state.connected = false })
  client.on('close', () => { state.connected = false })

  client.on('message', (topic, payloadBuffer) => {
    const text = payloadBuffer.toString()
    state.journal.push(`${topic} ${text}`)
    if (state.journal.length > 60) state.journal.shift()

    if (topic === `tvm/${tvmId}/etat`) {
      try { state.etat = JSON.parse(text).etat } catch { /* ignore */ }
    } else if (topic === `tvm/${tvmId}/caisse`) {
      try { state.caisse = JSON.parse(text) } catch { /* ignore */ }
    } else if (topic === `tvm/${tvmId}/vente`) {
      try {
        state.derniereVente = JSON.parse(text)
        state.dernierType = 'vente'
        state.dernierEvenementId++
      } catch { /* ignore */ }
    } else if (topic.startsWith(`tvm/${tvmId}/erreurs/`)) {
      try {
        state.derniereErreur = JSON.parse(text)
        state.dernierType = 'erreur'
        state.dernierEvenementId++
      } catch { /* ignore */ }
    } else if (topic === `tvm/${tvmId}/annulation`) {
      try {
        state.derniereAnnulation = JSON.parse(text)
        state.dernierType = 'annulation'
        state.dernierEvenementId++
      } catch { /* ignore */ }
    }
  })

  function publier(commande, payload = {}) {
    client.publish(`tvm/${tvmId}/commands/${commande}`, JSON.stringify(payload))
  }

  return { state, publier }
}
