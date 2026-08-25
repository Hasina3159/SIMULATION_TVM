<script setup>
defineProps({
  erreur: { type: Object, default: null },
})
</script>

<template>
  <div class="center-flex">
    <div class="icon-big">⚠️</div>
    <h1 class="screen-title">Une erreur est survenue</h1>
    <div class="erreur-box" v-if="erreur">
      {{ erreur.type }} — {{ erreur.message }}
    </div>
    <p class="screen-subtitle">Retour a l'accueil...</p>
  </div>
</template>
