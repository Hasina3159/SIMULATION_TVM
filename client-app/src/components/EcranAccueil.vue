<script setup>
defineEmits(['demarrer'])
</script>

<template>
  <div class="center-flex">
    <div class="icon-big">🎫</div>
    <h1 class="screen-title">Bienvenue</h1>
    <p class="screen-subtitle">Distributeur de titres de transport</p>
    <button class="btn btn-primary" style="text-align:center" @click="$emit('demarrer')">
      Touchez pour commencer
    </button>
  </div>
</template>
