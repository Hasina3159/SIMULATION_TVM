<script setup>
defineEmits(['demarrer'])
</script>

<template>
  <div class="center-flex">
    <svg class="icon-big" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <path d="M2 9a3 3 0 1 0 0 6v2a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-2a3 3 0 1 1 0-6V7a2 2 0 0 0-2-2H4a2 2 0 0 0-2 2Z" />
      <path d="M9 5v2M9 11v2M9 17v2" />
    </svg>
    <h1 class="screen-title">Bienvenue</h1>
    <p class="screen-subtitle">Distributeur de titres de transport</p>
    <button class="btn btn-primary" style="text-align:center" @click="$emit('demarrer')">
      Touchez pour commencer
    </button>
  </div>
</template>
