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

// true entre le moment ou on lance une transaction reseau et celui ou son
// resultat (vente/erreur/annulation) a ete affiche.
const enTransaction = ref(false)
let resultTimer = null
let idleReturnTimer = null

// Reagit directement a l'ARRIVEE d'un evenement de resultat, jamais a une
// transition d'etat : "vente" (QoS 2) et "etat" (QoS 1) n'ont aucune garantie
// d'ordre de livraison relatif venant du meme client MQTT, donc guetter
// "l'etat devient IDLE" pour verifier si un resultat est arrive est fragile.
watch(() => props.state.dernierEvenementId, () => {
  if (!enTransaction.value) return
  enTransaction.value = false
  clearTimeout(idleReturnTimer)
  etapeLocale.value = 'RESULTAT'
  clearTimeout(resultTimer)
  resultTimer = setTimeout(() => {
    etapeLocale.value = 'ACCUEIL'
    titreChoisi.value = null
    moyenPaiement.value = null
  }, 3500)
})

// Filet de securite pour une annulation SANS remboursement (rien insere) :
// aucun evenement dedie n'est publie dans ce cas. On laisse une petite marge
// apres le retour a IDLE au cas ou un evenement arriverait juste derriere.
watch(() => props.state.etat, (etat) => {
  if (etat !== 'IDLE' || !enTransaction.value) return
  clearTimeout(idleReturnTimer)
  idleReturnTimer = setTimeout(() => {
    if (!enTransaction.value) return
    enTransaction.value = false
    etapeLocale.value = 'ACCUEIL'
    titreChoisi.value = null
    moyenPaiement.value = null
  }, 300)
})

onUnmounted(() => {
  clearTimeout(resultTimer)
  clearTimeout(idleReturnTimer)
})

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
  enTransaction.value = true
  props.publier('selection_titre', {
    type_titre: titreChoisi.value.type_titre,
    quantite: 1,
  })
}

function annulerTransaction() {
  props.publier('annuler')
}

const ecranActif = computed(() => {
  // Une fois un resultat frais affiche, il reste prioritaire pendant sa
  // duree d'affichage, meme si le backend republie encore de l'etat brut
  // juste apres (ex: un dernier message IDLE qui arrive en retard).
  if (etapeLocale.value === 'RESULTAT') {
    if (props.state.dernierType === 'erreur') return 'erreur'
    if (props.state.dernierType === 'annulation') return 'annulation'
    return 'distribution'
  }

  const etat = props.state.etat

  if (etat === 'AWAITING_PAYMENT')
    return moyenPaiement.value === 'carte' ? 'carte' : 'especes'
  if (etat === 'VALIDATING_PAYMENT') return 'validation'
  if (etat === 'PRINTING') return 'impression'
  if (etat === 'DISPENSING') return 'distribution'
  if (etat === 'ERROR') return 'erreur'

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
