
<script setup>
const props = defineProps({
  erreur: { type: Object, default: null },
})

const MESSAGES = {
  refuse_solde_insuffisant: 'Solde insuffisant sur le compte',
  refuse_compte_inconnu: 'Compte inconnu',
  stock_monnaie_insuffisant: 'Rendu de monnaie impossible : especes rendues integralement',
  bourrage_papier: 'Bourrage papier a l\'impression',
}

function texteLisible(erreur) {
  if (!erreur) return ''
  return MESSAGES[erreur.message] || `${erreur.type} : ${erreur.message}`
}
</script>

<template>
  <div class="center-flex">
    <svg class="icon-big icon-error" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <path d="M10.29 3.86 1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0Z" />
      <path d="M12 9v4" />
      <path d="M12 17h.01" />
    </svg>
    <h1 class="screen-title">Une erreur est survenue</h1>
    <div class="erreur-box" v-if="erreur">
      {{ texteLisible(erreur) }}
    </div>
    <p class="screen-subtitle">Retour a l'accueil dans quelques secondes...</p>
  </div>
</template>
