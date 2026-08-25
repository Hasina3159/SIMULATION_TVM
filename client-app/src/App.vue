<script setup>
import { useTvmConnection } from './mqttClient.js'
import TvmInteraction from './components/TvmInteraction.vue'

const TVM_ID = 'tvm_01'
const { state, publier } = useTvmConnection(TVM_ID)
</script>

<template>
  <div class="kiosk-frame">
    <div class="kiosk-header">
      <span class="brand">TVM · {{ TVM_ID }}</span>
      <span>
        <span class="status-dot" :class="{ connected: state.connected }"></span>
        {{ state.connected ? 'connecte' : 'deconnecte' }}
      </span>
    </div>

    <div class="kiosk-screen">
      <TvmInteraction :state="state" :publier="publier" />
    </div>

    <div class="journal">
      <div v-for="(ligne, i) in state.journal.slice(0, 6)" :key="i">{{ ligne }}</div>
    </div>
  </div>
</template>
