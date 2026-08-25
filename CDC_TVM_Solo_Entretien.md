# Cahier des charges — TVM unique, démo entretien Conduent

**Version** 3.0 — Dérivé de `CDC_Simulateur_TVM_MQTT.md` (v2.0), périmètre volontairement réduit pour être réalisable rapidement tout en restant démontrable et explicable en entretien.
**Objectif de ce document** : ne garder que ce qui démontre les compétences visées (MQTT réaliste, concurrence C++, atomicité comptable) sur **un seul TVM**, en coupant tout ce qui est du volume sans valeur de démonstration proportionnelle.

---

## 1. Ce qui change par rapport à la v2.0

### 1.1 Explicitement supprimé
| Supprimé | Raison |
|---|---|
| `AuthManager` (login, hash+sel, sessions) | Complexité réelle (CRUD, sécurité) sans rapport avec MQTT/concurrence/caisse — les objectifs pédagogiques visés |
| Flotte de 10+ TVM, vue flotte | Un seul TVM suffit à démontrer la machine à états et le paiement |
| `ReservationManager` (réservation atomique multi-utilisateur) | N'existe que s'il y a plusieurs TVM/clients à arbitrer |
| Dashboard admin, endpoints admin | Rien à administrer avec un seul TVM et un seul compte |
| `RestApiServer` / API REST / `cpp-httplib` | Toute l'interaction (commandes, état, paiement) passe déjà par MQTT dans la v2.0 — sans auth ni réservation, aucun endpoint REST n'est encore nécessaire |
| Persistance de la caisse côté `collector` (`CashManager`, tables `tvm_registry`/`tvm_caisse`) | La caisse reste en mémoire dans le TVM (`CashDrawer`, déjà implémenté) ; la persistance "miroir" est un raffinement, pas un prérequis de démo |

### 1.2 Ce qui reste, et pourquoi c'est suffisant
- **Machine à états `TvmSupervisor`** pilotée uniquement par événements réels — démontre la modélisation d'un vrai TVM
- **`CashDrawer` + `calculer_rendu()`** — démontre l'atomicité comptable (rendu de monnaie, rollback)
- **`CardReader` + flux MQTT requête/réponse corrélé avec le `collector`** — démontre MQTT réaliste (QoS, corrélation, timeout) ET concurrence C++ (mutex/condition_variable entre le thread appelant et le thread de callback Paho)
- **Débit SQLite atomique côté `collector`** — démontre la même famille de problème que la caisse (transaction indivisible), avec un composant serveur minimal

C'est le sous-ensemble qui concentre l'essentiel de la valeur technique du CDC v2.0 original.

---

## 2. Compte utilisateur — version démo

Un seul compte, **codé en dur**, sans inscription ni connexion :
```
compte_id: "cl_demo"
solde initial: 50.00 €
```
Ce compte est **seedé directement en SQLite** au démarrage du `collector` (`INSERT OR IGNORE` si la table est vide), pas via un formulaire. Le frontend l'utilise tel quel, sans écran de login.

---

## 3. Cycle de vie du TVM (inchangé de la v2.0)

```
IDLE → SELECTING_TICKET → SUMMARIZING_ORDER → AWAITING_PAYMENT
     → VALIDATING_PAYMENT → PRINTING → DISPENSING → IDLE
                           ↘ ERROR → IDLE
```
Déjà implémenté dans [TvmSupervisor.cpp](tvm-simulator/TvmSupervisor.cpp) — transitions validées par une table `m_order`. **Reste à faire** : brancher les transitions sur de vraies commandes MQTT reçues (aujourd'hui `TvmSupervisor` n'est appelé que depuis `main.cpp`, pas encore depuis un dispatcher MQTT).

Pas de notion d'occupation/réservation : le TVM est toujours "libre" au sens où n'importe quelle commande MQTT reçue le fait avancer — il n'y a qu'un seul client possible de toute façon.

---

## 4. Paiement en espèces

Identique à la v2.0 section 5 — **entièrement local au TVM**, aucun aller-retour réseau nécessaire :
- Dénominations en centimes (`unsigned long long`), déjà modélisées dans [CashDrawer.hpp](tvm-simulator/CashDrawer.hpp)
- `calculer_rendu()` glouton, déjà implémenté et fonctionnel
- `retire_cash()` fusionne caisse + montant en attente, calcule le rendu, applique atomiquement ; `rollback()` restitue l'inséré si le rendu échoue

**État** : logique déjà écrite et couverte (à confirmer par des tests dédiés — voir section 9). **Reste à faire** : brancher sur les commandes MQTT `inserer_espece` / publication du résultat sur `tvm/{id}/vente`.

---

## 5. Paiement par carte

Identique à la v2.0 section 6 — c'est le cœur de la démonstration MQTT + concurrence.

### 5.1 Flux (déjà implémenté côté TVM)
```
tvm/{tvm_id}/paiement_compte/demande     (TVM → Collector)
tvm/{tvm_id}/paiement_compte/reponse     (Collector → TVM)
```
`CardReader` (voir [CardReader_NOTES.md](tvm-simulator/CardReader_NOTES.md)) publie la demande, attend une réponse corrélée par `correlation_id` avec timeout (5s par défaut), via un seul slot en attente (pas de map — un seul paiement en vol à la fois, cf. discussion sur YAGNI). **Fait** : implémentation + tests unitaires avec un `IMqttPublisher` factice (`tests/test_card_reader.cpp`).

### 5.2 Ce qu'il manque : le `collector`
Le `collector` actuel est un squelette vide. Pour ce périmètre, il doit uniquement :
1. Se connecter à Mosquitto, s'abonner à `tvm/+/paiement_compte/demande`
2. À réception, débiter `cl_demo` en SQLite dans une transaction indivisible (`BEGIN IMMEDIATE ... COMMIT`)
3. Publier la réponse sur `tvm/{tvm_id}/paiement_compte/reponse` avec le même `correlation_id`

Statuts possibles (inchangés de la v2.0) : `accepte`, `refuse_solde_insuffisant`, `refuse_compte_inconnu` (n'arrivera jamais en pratique avec un seul compte, mais le code doit rester correct si `compte_id` ne correspond pas à `cl_demo`), `timeout` (côté TVM uniquement, si le `collector` ne répond pas).

---

## 6. Stack technique (réduite)

| Composant | v2.0 | v3.0 (ce périmètre) |
|---|---|---|
| Broker | Mosquitto | inchangé |
| TVM (C++17) | Paho MQTT C++, nlohmann/json, threads | inchangé |
| Collector (C++17) | + SQLite3, cpp-httplib, AuthManager, ReservationManager, CashManager | **SQLite3 uniquement** (`ComptesManager` + écoute MQTT) — pas de `cpp-httplib` |
| Frontend | Vue 3, mqtt.js, axios/fetch, 2 apps | **1 app**, un écran par état de la machine à états, mqtt.js uniquement (pas d'appel REST), visuel inspiré d'une vraie borne (façon Conduent) — HTML/CSS soigné, priorité au réalisme du parcours |

`nlohmann/json` doit être vendoré dans `third_party/` (fait). `libsqlite3-dev` à installer si pas déjà présent.

---

## 6bis. Frontend — parcours complet, un écran par état

Contrairement à l'idée initiale d'un écran unique minimal, le frontend doit couvrir **tous les états** de `TvmSupervisor`, pour ressembler à un vrai parcours de borne (et non une simple démo technique). Chaque état a son propre écran, affiché selon la valeur reçue en direct sur `tvm/{id}/etat` (topic retained) :

| État `TvmSupervisor` | Écran |
|---|---|
| `IDLE` | Accueil ("Touchez pour commencer") |
| `SELECTING_TICKET` | Choix type de titre / zone / quantité |
| `SUMMARIZING_ORDER` | Récapitulatif + choix du moyen de paiement (espèces/carte) |
| `AWAITING_PAYMENT` | Écran espèces (dénominations, montant restant) ou écran carte (attente), selon le choix précédent |
| `VALIDATING_PAYMENT` | "Traitement en cours..." |
| `PRINTING` | "Impression en cours..." |
| `DISPENSING` | "Récupérez votre titre" → retour `IDLE` |
| `ERROR` | Message d'erreur (bourrage, stock insuffisant...) → retour `IDLE` |

Composants Vue correspondants (déjà ébauchés en stub dans `client-app/src/components/` — à compléter, pas tous nécessairement un par un, `TvmInteraction.vue` peut orchestrer l'affichage conditionnel) : `PaiementEspeces.vue`, `PaiementCarte.vue`, + composants d'écran manquants pour les autres états.

Le style visuel (couleurs, disposition, typographie) doit s'inspirer d'une vraie borne de type Conduent/Genfare — le HTML/CSS n'est pas un facteur limitant ici, c'est la partie la plus solide du profil de l'auteur du projet.

---

## 7. Topics MQTT utilisés (sous-ensemble de la v2.0 section 9)

```
tvm/{tvm_id}/etat
tvm/{tvm_id}/vente
tvm/{tvm_id}/caisse
tvm/{tvm_id}/erreurs/{type_erreur}
tvm/{tvm_id}/heartbeat
tvm/{tvm_id}/commands/selection_titre        # client → TVM
tvm/{tvm_id}/commands/inserer_espece         # client → TVM
tvm/{tvm_id}/commands/payer_carte            # client → TVM
tvm/{tvm_id}/commands/annuler                # client → TVM
tvm/{tvm_id}/paiement_compte/demande         # TVM → Collector
tvm/{tvm_id}/paiement_compte/reponse         # Collector → TVM
```
Retiré : `tvm/{id}/occupation`, `commands/admin/approvisionner`, `alerts/*`.

QoS/retained : inchangés de la v2.0 (section 9) pour les topics conservés.

---

## 8. Architecture logicielle

### 8.1 TVM — état d'implémentation
| Module | État |
|---|---|
| `TvmSupervisor` | ✓ machine à états, à brancher sur MQTT |
| `CashDrawer` + `calculer_rendu()` | ✓ implémenté |
| `CardReader` | ✓ implémenté + testé |
| `PrinterUnit` | ✓ implémenté |
| `HeartbeatPublisher` | ✓ implémenté |
| `MqttPublisher` / `MqttSubscriber` / `Callback` | ✓ implémentés |
| Dispatcher central (main/orchestrateur) | ✗ à écrire — relie les commandes MQTT entrantes aux bons modules et pilote `TvmSupervisor` |

### 8.2 Collector — à écrire
- `ComptesManager` : débit atomique SQLite (`BEGIN IMMEDIATE`), seed du compte `cl_demo` au démarrage
- `MqttListener` : abonnement à `paiement_compte/demande`, callback → `ComptesManager` → publication de la réponse
- Pas de `StorageWriter` séparé ni de `SafeQueue`/pool de workers dans un premier temps — un seul TVM ne génère pas assez de trafic pour justifier cette infrastructure ; à ajouter seulement si ça devient un vrai goulot d'étranglement

### 8.3 Schéma SQLite minimal
```sql
CREATE TABLE comptes (
    compte_id TEXT PRIMARY KEY,
    solde REAL NOT NULL DEFAULT 0
);

CREATE TABLE mouvements_solde (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    compte_id TEXT NOT NULL,
    montant REAL NOT NULL,
    solde_apres REAL NOT NULL,
    transaction_id TEXT,
    timestamp INTEGER NOT NULL
);
```
Pas de table `sessions`, `tvm_registry`, `tvm_caisse`, `alertes` pour ce périmètre.

---

## 9. Tests à avoir (priorité pour la démo)

1. `calculer_rendu()` — déjà couvrable, à écrire si pas encore fait
2. `CardReader` — ✓ déjà fait (`tests/test_card_reader.cpp`)
3. `ComptesManager` — débit atomique, refus si solde insuffisant, solde inchangé en cas de refus (reprendre l'exemple de la v2.0 section 18.2)
4. Test d'intégration bout-en-bout (optionnel mais fort en démo) : lancer `collector` + `tvm-simulator` + `mosquitto_pub` d'une commande `payer_carte`, vérifier la réponse et le nouveau solde en SQLite

---

## 10. Plan de développement réaliste

| Étape | Contenu | Estimation |
|---|---|---|
| 1 | `collector` : `ComptesManager` + seed + `MqttListener` sur `paiement_compte/demande` | 2–3h |
| 2 | Dispatcher TVM : brancher `TvmSupervisor` + `CashDrawer` + `CardReader` sur les commandes MQTT réelles | 2–3h |
| 3 | Tests `ComptesManager` + test d'intégration bout-en-bout | 1–2h |
| 4 | Frontend : parcours complet Vue (un écran par état, style borne réaliste) | 10–14h |

**Total estimé : ≈ 15–21h**, contre 28h+ pour la v2.0 complète. Le poste le plus incertain est le frontend (Vue + mqtt.js nouveaux pour l'auteur), le HTML/CSS n'étant pas un facteur de risque.

---

## 11. Critères d'acceptation (démo)

- [ ] Le `collector` démarre, seed `cl_demo` à 50€ si la table est vide
- [ ] Un paiement carte avec solde suffisant débite réellement le compte en SQLite et renvoie `accepte` + nouveau solde au TVM
- [ ] Un paiement carte avec solde insuffisant est refusé, le solde reste inchangé
- [ ] Un paiement espèces avec rendu de monnaie impossible restitue intégralement les espèces insérées
- [ ] Le TVM réagit à de vraies commandes MQTT (pas d'appel direct depuis `main.cpp`)
- [ ] Le frontend couvre tous les états de `TvmSupervisor` avec un écran dédié par état, visuellement inspiré d'une vraie borne, et permet de dérouler un parcours complet (sélection → paiement carte ou espèces → impression → distribution)
- [ ] Code compile sans warning (`-Wall -Wextra`)
- [ ] Tu peux expliquer, sans notes, pourquoi `CardReader` a besoin d'un mutex/cv et pourquoi un seul slot suffit (pas de map)

---

## 12. Ce qui reste optionnel / pour plus tard (hors périmètre entretien)

Tout ce qui a été coupé en section 1.1 reste dans `CDC_Simulateur_TVM_MQTT.md` comme ambition à plus long terme : auth, flotte, réservation, dashboard admin, persistance caisse côté collector. Rien de tout ça n'est nécessaire pour la démo — à ne reprendre que si le temps le permet après le socle de ce document.
