<script setup>
import { ref, computed, watch, onUnmounted } from 'vue'
import EcranAccueil from './EcranAccueil.vue'
import EcranChoixTitre from './EcranChoixTitre.vue'
import EcranRecapitulatif from './EcranRecapitulatif.vue'
import PaiementEspeces from './PaiementEspeces.vue'
import PaiementCarte from './PaiementCarte.vue'
import EcranValidation from './EcranValidation.vue'
import EcranImpression from './EcranImpression.vue'
import EcranDistribution from './EcranDistribution.vue'
import EcranErreur from './EcranErreur.vue'
import EcranAnnulation from './EcranAnnulation.vue'

const props = defineProps({
  state: { type: Object, required: true },
  publier: { type: Function, required: true },
})

// La commande "selection_titre" du CDC porte deja tout le necessaire (type,
// quantite) et fait avancer le TVM d'un coup jusqu'a AWAITING_PAYMENT : il
// n'y a pas d'aller-retour reseau distinct pour "choisir le titre" puis
// "voir le recap". Ces deux ecrans sont donc geres localement, avant meme
// d'envoyer quoi que ce soit au TVM (qui reste IDLE pendant ce temps).
const etapeLocale = ref('ACCUEIL') // ACCUEIL | CHOIX_TITRE | RECAPITULATIF | RESULTAT
const titreChoisi = ref(null)
const moyenPaiement = ref(null)

// Id de reference pris au moment ou une transaction reseau demarre, pour
// savoir si un evenement (vente/erreur/annulation) frais est arrive PENDANT
// cette transaction avant de decider s'il y a un resultat a montrer.
let idAvantTransaction = 0
let resultTimer = null

watch(() => props.state.etat, (etat, ancienEtat) => {
  if (etat !== 'IDLE') return

  clearTimeout(resultTimer)
  if (props.state.dernierEvenementId > idAvantTransaction) {
    etapeLocale.value = 'RESULTAT'
    resultTimer = setTimeout(() => {
      etapeLocale.value = 'ACCUEIL'
      titreChoisi.value = null
      moyenPaiement.value = null
    }, 3500)
  } else if (ancienEtat !== undefined) {
    etapeLocale.value = 'ACCUEIL'
    titreChoisi.value = null
    moyenPaiement.value = null
  }
})

onUnmounted(() => clearTimeout(resultTimer))

function demarrer() {
  etapeLocale.value = 'CHOIX_TITRE'
}

function choisirTitre(titre) {
  titreChoisi.value = titre
  etapeLocale.value = 'RECAPITULATIF'
}

function annulerLocal() {
  etapeLocale.value = 'ACCUEIL'
  titreChoisi.value = null
}

function confirmerPaiement(moyen) {
  moyenPaiement.value = moyen
  idAvantTransaction = props.state.dernierEvenementId
  props.publier('selection_titre', {
    type_titre: titreChoisi.value.type_titre,
    quantite: 1,
  })
}

function annulerTransaction() {
  props.publier('annuler')
}

const ecranActif = computed(() => {
  const etat = props.state.etat

  if (etat === 'AWAITING_PAYMENT')
    return moyenPaiement.value === 'carte' ? 'carte' : 'especes'
  if (etat === 'VALIDATING_PAYMENT') return 'validation'
  if (etat === 'PRINTING') return 'impression'
  if (etat === 'DISPENSING') return 'distribution'
  if (etat === 'ERROR') return 'erreur'

  if (etapeLocale.value === 'RESULTAT') {
    if (props.state.dernierType === 'erreur') return 'erreur'
    if (props.state.dernierType === 'annulation') return 'annulation'
    return 'distribution'
  }
  if (etapeLocale.value === 'CHOIX_TITRE') return 'choix_titre'
  if (etapeLocale.value === 'RECAPITULATIF') return 'recapitulatif'
  return 'accueil'
})
</script>

<template>
  <EcranAccueil v-if="ecranActif === 'accueil'" @demarrer="demarrer" />

  <EcranChoixTitre
    v-else-if="ecranActif === 'choix_titre'"
    @choisir="choisirTitre"
    @annuler="annulerLocal"
  />

  <EcranRecapitulatif
    v-else-if="ecranActif === 'recapitulatif'"
    :titre="titreChoisi"
    @payer="confirmerPaiement"
    @annuler="annulerLocal"
  />

  <PaiementEspeces
    v-else-if="ecranActif === 'especes'"
    :prix="titreChoisi?.prix ?? 0"
    :caisse="state.caisse"
    :publier="publier"
    @annuler="annulerTransaction"
  />

  <PaiementCarte
    v-else-if="ecranActif === 'carte'"
    :prix="titreChoisi?.prix ?? 0"
    :publier="publier"
    @annuler="annulerTransaction"
  />

  <EcranValidation v-else-if="ecranActif === 'validation'" />
  <EcranImpression v-else-if="ecranActif === 'impression'" />
  <EcranDistribution v-else-if="ecranActif === 'distribution'" :vente="state.derniereVente" />
  <EcranErreur v-else-if="ecranActif === 'erreur'" :erreur="state.derniereErreur" />
  <EcranAnnulation v-else-if="ecranActif === 'annulation'" :annulation="state.derniereAnnulation" />
</template>
