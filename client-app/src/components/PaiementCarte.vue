<script setup>
import { ref } from 'vue'

const props = defineProps({
  prix: { type: Number, required: true },
  publier: { type: Function, required: true },
})
defineEmits(['annuler'])

const compteId = ref('cl_demo')
const enAttente = ref(false)

function formatPrix(centimes) {
  return (centimes / 100).toFixed(2) + ' €'
}

function payer() {
  enAttente.value = true
  props.publier('payer_carte', { compte_id: compteId.value })
}
</script>

<template>
  <div>
    <h1 class="screen-title">Paiement par carte</h1>
    <p class="screen-subtitle">Montant a debiter : {{ formatPrix(prix) }}</p>
  </div>
  <div class="screen-body">
    <input v-model="compteId" placeholder="compte_id" :disabled="enAttente"
           style="padding:12px;border-radius:8px;border:1px solid #ccd6e0;font-size:1rem;" />

    <button class="btn btn-primary" :disabled="enAttente" @click="payer">
      {{ enAttente ? 'Debit en cours...' : 'Valider le paiement' }}
    </button>

    <div v-if="enAttente" class="spinner"></div>

    <button class="btn btn-cancel" :disabled="enAttente" @click="$emit('annuler')">Annuler</button>
  </div>
</template>
