<script setup>
const props = defineProps({
  annulation: { type: Object, default: null },
})

function formatPrix(montantEuros) {
  return montantEuros.toFixed(2) + ' €'
}
</script>

<template>
  <div class="center-flex">
    <svg class="icon-big" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <circle cx="12" cy="12" r="10" />
      <path d="M9.5 9.5 14.5 14.5M14.5 9.5 9.5 14.5" />
    </svg>
    <h1 class="screen-title">Transaction annulee</h1>
    <div v-if="annulation && annulation.montant_rendu > 0" class="montant-box" style="width:100%">
      <span>Especes rendues</span>
      <span class="valeur">{{ formatPrix(annulation.montant_rendu) }}</span>
    </div>
    <p class="screen-subtitle">Retour a l'accueil dans quelques secondes...</p>
  </div>
</template>
