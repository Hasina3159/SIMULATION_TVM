<script setup>
const emit = defineEmits(['choisir', 'annuler'])

// Catalogue statique, en centimes — doit correspondre a m_catalogue dans
// TvmOrchestrator.cpp.
const CATALOGUE = [
  { type_titre: 'ticket_unique', label: 'Ticket unique', prix: 200 },
  { type_titre: 'carnet_10', label: 'Carnet 10 tickets', prix: 1600 },
]

function formatPrix(centimes) {
  return (centimes / 100).toFixed(2) + ' €'
}
</script>

<template>
  <div>
    <h1 class="screen-title">Choisissez un titre</h1>
    <p class="screen-subtitle">Selectionnez le titre de transport souhaite</p>
  </div>
  <div class="screen-body">
    <button
      v-for="titre in CATALOGUE"
      :key="titre.type_titre"
      class="btn btn-secondary"
      @click="emit('choisir', titre)"
    >
      {{ titre.label }}
      <span class="price-tag"> — {{ formatPrix(titre.prix) }}</span>
    </button>
    <button class="btn btn-cancel" @click="emit('annuler')">Annuler</button>
  </div>
</template>
