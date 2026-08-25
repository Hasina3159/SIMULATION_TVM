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

    <button class="btn btn-cash" @click="emit('payer', 'especes')">
      <svg class="btn-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
        <rect x="2" y="6" width="20" height="12" rx="2" />
        <circle cx="12" cy="12" r="3" />
        <path d="M6 12h.01M18 12h.01" />
      </svg>
      Payer en especes
    </button>
    <button class="btn btn-primary" @click="emit('payer', 'carte')">
      <svg class="btn-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
        <rect x="2" y="5" width="20" height="14" rx="2" />
        <line x1="2" y1="10" x2="22" y2="10" />
      </svg>
      Payer par carte
    </button>
    <button class="btn btn-cancel" @click="emit('annuler')">Annuler</button>
  </div>
</template>
