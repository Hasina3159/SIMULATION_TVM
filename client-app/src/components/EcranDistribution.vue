<script setup>
defineProps({
  vente: { type: Object, default: null },
})

function formatPrix(montantEuros) {
  return montantEuros.toFixed(2) + ' €'
}
</script>

<template>
  <div class="center-flex">
    <svg class="icon-big icon-success" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <circle cx="12" cy="12" r="10" />
      <path d="m8 12 2.5 2.5L16 9" />
    </svg>
    <h1 class="screen-title">Recuperez votre titre</h1>
    <p v-if="vente" class="screen-subtitle">
      {{ vente.type_titre }} : {{ formatPrix(vente.prix_total) }} ({{ vente.mode_paiement }})
    </p>
    <div v-if="vente && vente.monnaie_rendue > 0" class="montant-box" style="width:100%">
      <span>Monnaie rendue</span>
      <span class="valeur">{{ formatPrix(vente.monnaie_rendue) }}</span>
    </div>
    <p class="screen-subtitle">Merci de votre achat</p>
  </div>
</template>
