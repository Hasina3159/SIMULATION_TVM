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
    <div class="icon-big">✅</div>
    <h1 class="screen-title">Recuperez votre titre</h1>
    <p v-if="vente" class="screen-subtitle">
      {{ vente.type_titre }} — {{ formatPrix(vente.prix_total) }} ({{ vente.mode_paiement }})
    </p>
    <p class="screen-subtitle">Merci de votre achat</p>
  </div>
</template>
