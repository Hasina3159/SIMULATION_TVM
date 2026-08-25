<script setup>
import { ref, computed } from 'vue'

const props = defineProps({
  prix: { type: Number, required: true },
  publier: { type: Function, required: true },
})
defineEmits(['annuler'])

const DENOMINATIONS = [10, 20, 50, 100, 200, 500, 1000, 2000, 5000]
const montantInsere = ref(0)

const montantRestant = computed(() => Math.max(props.prix - montantInsere.value, 0))

function formatPrix(centimes) {
  return (centimes / 100).toFixed(2) + ' €'
}

function inserer(denomination) {
  montantInsere.value += denomination
  props.publier('inserer_espece', { denomination })
}
</script>

<template>
  <div>
    <h1 class="screen-title">Paiement en especes</h1>
    <p class="screen-subtitle">Inserez vos pieces et billets</p>
  </div>
  <div class="screen-body">
    <div class="montant-box">
      <span>Insere</span>
      <span class="valeur">{{ formatPrix(montantInsere) }}</span>
    </div>
    <div class="montant-box">
      <span>Restant du</span>
      <span class="valeur">{{ formatPrix(montantRestant) }}</span>
    </div>

    <div class="denom-grid">
      <button
        v-for="d in DENOMINATIONS"
        :key="d"
        class="btn btn-secondary"
        style="text-align:center"
        @click="inserer(d)"
      >
        {{ formatPrix(d) }}
      </button>
    </div>

    <button class="btn btn-cancel" @click="$emit('annuler')">Annuler (rend les especes)</button>
  </div>
</template>
