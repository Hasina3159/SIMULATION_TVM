<script setup>
const props = defineProps({
  titre: { type: Object, required: true },
})
const emit = defineEmits(['payer', 'annuler'])

function formatPrix(centimes) {
  return (centimes / 100).toFixed(2) + ' €'
}
</script>

<template>
  <div>
    <h1 class="screen-title">Recapitulatif</h1>
    <p class="screen-subtitle">Verifiez votre commande puis choisissez le paiement</p>
  </div>
  <div class="screen-body">
    <div class="recap-card">
      <div class="type">{{ titre.label }}</div>
      <div class="prix">{{ formatPrix(titre.prix) }}</div>
    </div>

    <button class="btn btn-cash" @click="emit('payer', 'especes')">💶 Payer en especes</button>
    <button class="btn btn-primary" @click="emit('payer', 'carte')">💳 Payer par carte</button>
    <button class="btn btn-cancel" @click="emit('annuler')">Annuler</button>
  </div>
</template>
