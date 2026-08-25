<script setup>
import { ref, watch, nextTick } from 'vue'
import { useTvmConnection } from './mqttClient.js'
import TvmInteraction from './components/TvmInteraction.vue'

const TVM_ID = 'tvm_01'
const { state, publier } = useTvmConnection(TVM_ID)

const journalEl = ref(null)
watch(() => state.journal.length, () => {
  nextTick(() => {
    if (journalEl.value)
      journalEl.value.scrollTop = journalEl.value.scrollHeight
  })
})
</script>

<template>
  <div class="kiosk-frame">
    <div class="kiosk-header">
      <span class="brand">TVM · {{ TVM_ID }}</span>
      <span class="etat-badge">{{ state.etat }}</span>
      <span>
        <span class="status-dot" :class="{ connected: state.connected }"></span>
        {{ state.connected ? 'connecte' : 'deconnecte' }}
      </span>
    </div>

    <div class="kiosk-screen">
      <TvmInteraction :state="state" :publier="publier" />
    </div>

    <div class="journal" ref="journalEl">
      <div v-for="(ligne, i) in state.journal" :key="i">{{ ligne }}</div>
    </div>
  </div>
</template>
