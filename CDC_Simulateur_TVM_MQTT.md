# Cahier des charges — Plateforme de flotte de TVM connectés (MQTT + C++ multi-thread + Vue.js)

**Version** 2.0 — Refonte : suppression du mode autonome aléatoire, ajout comptes réels (login/mdp), réservation de machine, caisse réelle par TVM, deux moyens de paiement effectifs (espèces et carte)
**Environnement de développement/test** : Ubuntu (poste unique, aucun matériel physique)

---

## 1. Contexte et objectif

### 1.1 Contexte
Le projet reproduit l'architecture logicielle d'une **flotte de distributeurs automatiques de titres de transport (TVM)**, façon Conduent/Genfare, avec un principe directeur central : **chaque TVM se comporte comme une vraie machine**, jamais comme un générateur de données factices. Un TVM reste `IDLE` tant que personne ne l'utilise, ne fait rien de son propre chef, et ne réagit qu'à de véritables actions d'un utilisateur connecté.

### 1.2 Principes fondateurs (ce qui change fondamentalement par rapport à une v1 "simulation aléatoire")
- **Aucune transaction générée automatiquement.** Un TVM ne "joue" jamais seul un scénario d'achat. Le seul aléa toléré est au niveau de la simulation matérielle (ex: probabilité de bourrage papier), jamais au niveau du parcours d'achat lui-même.
- **Chaque TVM a sa propre caisse réelle** : un stock fini de pièces et billets par dénomination, qui évolue réellement (encaissement à l'insertion, décrément au rendu de monnaie), et peut s'épuiser.
- **Chaque utilisateur a un vrai compte** (email + mot de passe), avec un solde persistant utilisable comme moyen de paiement ("carte").
- **Un TVM ne peut être utilisé que par un seul utilisateur à la fois** — il faut le réserver depuis la vue flotte avant d'interagir avec.
- **Deux moyens de paiement réellement simulés** : espèces (dénominations physiques, rendu de monnaie depuis la caisse du TVM) et carte (débit du solde du compte utilisateur).
- **Un dashboard central admin** donne une vue d'ensemble de l'état de chaque TVM (occupation, caisse) et de chaque compte utilisateur (solde), et permet d'approvisionner les TVM en espèces et de recharger les comptes utilisateurs.

### 1.3 Objectifs pédagogiques
- MQTT réaliste : topics hiérarchiques, QoS différencié, retained, LWT, pattern requête/réponse corrélé
- Concurrence C++ : threads, mutex/condition_variable, verrouillage/réservation atomique (au-delà du simple producteur/consommateur)
- Modélisation d'un vrai système de caisse (dénominations, rendu de monnaie, atomicité comptable)
- Authentification et gestion de session basique côté API REST

### 1.4 Contraintes
- Un seul poste de développement (Ubuntu), pas de matériel IoT physique
- Aucune dépendance payante ou nécessitant un compte cloud
- Tout tourne en local (localhost)

### 1.5 Hors périmètre (explicitement exclu)
- Vraie sécurité bancaire (chiffrement PCI-DSS, HTTPS obligatoire, 2FA) — l'authentification reste volontairement simplifiée (voir section 13, piège d'authentification)
- Haute disponibilité / clustering du broker MQTT
- Gestion multi-langue réelle (i18n)
- Vraie gestion électromécanique (moteurs de rendu de monnaie physiques, capteurs)

---

## 2. Rôles et comptes utilisateurs

### 2.1 Deux rôles
| Rôle | Peut faire |
|---|---|
| `client` | Se connecter, consulter la flotte, réserver un TVM libre, effectuer un achat (espèces ou carte), recharger son propre solde, consulter son historique |
| `admin` | Tout ce que peut faire un client, + consulter le dashboard central (tous les TVM, tous les comptes), approvisionner un TVM en espèces, recharger le solde de n'importe quel compte |

### 2.2 Création de compte et authentification
- Formulaire d'inscription : nom, email, mot de passe
- Le mot de passe n'est **jamais stocké en clair** : hashé avec sel (`SHA-256` salé au minimum — OpenSSL déjà disponible car nécessaire à Paho pour TLS ; une bibliothèque dédiée comme `libsodium`/`bcrypt` serait préférable en vrai produit mais hors scope ici, voir piège en section 13)
- À l'inscription, un **solde initial aléatoire** est attribué (ex: 20€ à 100€)
- Connexion : email + mot de passe → un **jeton de session** est généré (chaîne aléatoire, stocké côté serveur avec une expiration, ex 2h) et renvoyé au frontend, à fournir dans l'en-tête `Authorization` de chaque requête REST authentifiée
- Un compte `admin` de démonstration est créé au premier lancement (seed), avec un mot de passe à changer immédiatement en environnement réel — acceptable tel quel pour ce projet personnel

---

## 3. Cycle de vie et disponibilité d'un TVM

### 3.1 État machine à états (identique dans son principe à la v1, mais jamais déclenché seul)

```
IDLE → SELECTING_TICKET → SUMMARIZING_ORDER → AWAITING_PAYMENT
     → VALIDATING_PAYMENT → PRINTING → DISPENSING → IDLE
                           ↘ ERROR → IDLE
```

Chaque transition est déclenchée **exclusivement** par un événement provenant :
- d'une commande MQTT envoyée par l'utilisateur actuellement connecté à ce TVM (sélection de titre, insertion d'espèces, demande de paiement carte)
- d'une réponse d'un sous-système simulé (lecteur carte, imprimante) — dont le **délai** peut être aléatoire (latence réseau/matérielle réaliste) mais dont le **déclenchement** ne l'est jamais

### 3.2 Disponibilité — un TVM par utilisateur à la fois

Chaque TVM a un état de disponibilité indépendant de sa machine à états fonctionnelle :

| Disponibilité | Signification |
|---|---|
| `libre` | Aucun utilisateur connecté ne l'utilise actuellement, visible/sélectionnable dans la vue flotte |
| `occupe` | Un utilisateur (identifié par `compte_id`) est en train de l'utiliser — invisible/non sélectionnable pour les autres |

**Réservation** : un client sélectionne un TVM `libre` depuis la vue flotte → appel `POST /api/tvm/{id}/reserver` → le TVM passe en `occupe` avec le `compte_id` du demandeur, **de façon atomique** (voir piège en section 13 — deux clients ne doivent jamais pouvoir réserver le même TVM simultanément).

**Libération** : automatique dans 3 cas :
1. Achat terminé (retour à `IDLE` après `DISPENSING` ou `ERROR`) et le client quitte volontairement l'écran
2. Le client clique explicitement "Quitter" avant la fin
3. **Timeout d'inactivité** (ex: 3 minutes sans commande reçue) — un thread dédié du Collector vérifie périodiquement `derniere_activite` et libère automatiquement les TVM abandonnés, pour éviter qu'un utilisateur parti sans se déconnecter bloque une machine indéfiniment

---

## 4. Parcours d'achat détaillé

1. Le client se connecte (email + mot de passe)
2. Il consulte la **vue flotte** : liste des TVM avec leur disponibilité (`libre`/`occupe`) et leur état (`IDLE`, en cours d'utilisation, hors ligne, erreur)
3. Il réserve un TVM `libre`
4. Il sélectionne un titre (type, zone, quantité) — le TVM passe en `SELECTING_TICKET` puis `SUMMARIZING_ORDER`
5. Il choisit son moyen de paiement : **espèces** ou **carte**
6. Selon le moyen choisi, déroulé détaillé en section 5 (espèces) ou section 6 (carte)
7. Impression du titre (avec risque simulé de bourrage papier — probabilité fixe, ex 2%)
8. Distribution, le TVM revient en `IDLE`, redevient `libre`

---

## 5. Paiement en espèces (réel, par dénominations)

### 5.1 Dénominations simulées
Pièces : 0.10€, 0.20€, 0.50€, 1€, 2€ — Billets : 5€, 10€, 20€, 50€

### 5.2 Insertion
Le client "insère" de l'argent dénomination par dénomination depuis l'interface (boutons représentant chaque pièce/billet, comme sur un vrai TVM). Chaque clic envoie une commande MQTT `tvm/{id}/commands/inserer_espece` avec la dénomination insérée.

**Effet immédiat, réaliste** : dès l'insertion, cette dénomination est ajoutée au stock de caisse du TVM (comme sur une vraie machine — l'argent inséré est physiquement dans la caisse dès l'insertion, avant même de savoir si la transaction aboutira).

### 5.3 Validation et rendu de monnaie
Quand le montant inséré ≥ prix du titre, le TVM calcule la monnaie à rendre = `montant_inséré - prix`, puis tente de la composer à partir de son **propre stock de dénominations disponibles**, en utilisant un algorithme glouton (des plus grosses aux plus petites coupures) :

```cpp
// Retourne le détail du rendu si possible, sinon std::nullopt
std::optional<std::map<double,int>> calculer_rendu(
    double montant_a_rendre,
    const std::map<double,int>& stock_disponible
);
```

**Si le rendu est impossible** (stock insuffisant dans les petites coupures nécessaires) : la transaction échoue avec l'erreur `stock_monnaie_insuffisant`. Comme l'argent a déjà été physiquement "inséré" (ajouté au stock), le TVM doit **annuler l'opération et rendre intégralement les espèces insérées** (jamais les garder), ce qui revient à défaire l'ajout au stock effectué en 5.2. C'est un exercice d'atomicité comptable : soit toute la transaction (insertion + rendu) réussit, soit tout est annulé proprement.

### 5.4 Effet net sur la caisse du TVM en cas de succès
- **+** chaque dénomination insérée par le client
- **−** chaque dénomination utilisée pour le rendu de monnaie

Chaque mouvement est journalisé dans `mouvements_caisse` (voir section 11.3), jamais uniquement reflété dans un total global — même logique de "grand livre" que pour les comptes utilisateurs.

---

## 6. Paiement par carte (débit du compte utilisateur)

Le paiement "carte" débite le **solde du compte du client actuellement connecté** — pas de simulation de banque externe, le compte de la plateforme fait à la fois office de carte de paiement et de carte de transport rechargeable.

### 6.1 Flux requête/réponse MQTT (le TVM ne débite jamais lui-même)

Le solde vit dans la base du Collector, pas dans le TVM. Le TVM envoie une **demande de débit** et attend une **réponse corrélée**, avec timeout :

```
tvm/{tvm_id}/paiement_compte/demande     (TVM → Collector)
tvm/{tvm_id}/paiement_compte/reponse     (Collector → TVM)
```

Demande :
```json
{
  "correlation_id": "req-8f3a1c",
  "compte_id": "cl_92ab",
  "montant": 14.50,
  "transaction_id": "a1b2c3d4",
  "timestamp": 1755680400
}
```

Réponse :
```json
{
  "correlation_id": "req-8f3a1c",
  "statut": "accepte",
  "nouveau_solde": 12.30,
  "timestamp": 1755680401
}
```
Valeurs de `statut` : `accepte`, `refuse_solde_insuffisant`, `refuse_compte_inconnu`, `timeout`

Si aucune réponse n'arrive dans le délai (ex 5s), le TVM traite la transaction comme un échec — jamais comme un succès implicite.

### 6.2 Atomicité du débit
Comme en v1, le débit doit être une transaction SQLite indivisible (`BEGIN IMMEDIATE` avant lecture du solde, jusqu'au `COMMIT` après écriture) pour empêcher un double débit en cas d'appels concurrents.

---

## 7. Administration

### 7.1 Approvisionnement d'un TVM en espèces
Un admin choisit un TVM et une quantité par dénomination à ajouter à sa caisse :
```
POST /api/admin/tvm/{tvm_id}/approvisionner
{ "0.10": 50, "0.20": 30, "1": 20, "2": 20, "5": 10, "10": 10, "20": 5, "50": 2 }
```
Chaque ajout est journalisé dans `mouvements_caisse` avec le type `approvisionnement`.

### 7.2 Recharge d'un compte utilisateur par un admin
```
POST /api/admin/comptes/{compte_id}/recharger
{ "montant": 25.00 }
```
Journalisé dans `mouvements_solde` avec le type `recharge_admin` et l'identifiant de l'admin ayant effectué l'opération (traçabilité).

### 7.3 Dashboard central admin
Vue agrégée en un seul écran :
- **Tableau des TVM** : identifiant, disponibilité (`libre`/`occupe` + par qui), état courant, niveau de caisse total et par dénomination, dernière activité
- **Tableau des comptes** : identifiant, nom, solde, date de création, dernière connexion
- Boutons d'action directs : approvisionner un TVM, recharger un compte, forcer la libération d'un TVM bloqué (cas exceptionnel, ex: bug applicatif)

---

## 8. Stack technique complète

### 8.1 Broker MQTT
- Eclipse Mosquitto — `tcp://localhost:1883` (interne) + `ws://localhost:9001` (frontend navigateur)

### 8.2 Simulateurs de TVM (C++17)
- Eclipse Paho MQTT C++ (`paho-mqttpp3` + `paho-mqtt3as`)
- nlohmann/json (header-only)
- `std::thread`, `std::mutex`, `std::condition_variable`, `std::atomic`
- CMake ≥ 3.16

### 8.3 Service Collector (C++17)
- Mêmes bibliothèques de base
- SQLite3 (`libsqlite3-dev`), mode WAL
- cpp-httplib (API REST, header-only)
- OpenSSL `libcrypto` (déjà présent pour TLS Paho) pour le hash salé des mots de passe
- SafeQueue + pool de workers (pattern déjà maîtrisé)
- Thread dédié de **libération automatique par timeout** des TVM inactifs

### 8.4 Frontend (Vue.js)
- Vue 3 (Composition API)
- `mqtt` (mqtt.js) en WebSocket direct
- `fetch`/`axios` pour l'API REST (auth, réservation, admin)
- Chart.js en option pour les historiques
- **Deux applications distinctes** : `client-app` (login, flotte, interaction TVM) et `admin-dashboard` (vue centrale)

### 8.5 Outils de développement
- `mosquitto_sub`/`mosquitto_pub`, `sqlite3` CLI, `ThreadSanitizer` (`-fsanitize=thread`)

---

## 9. Schéma des topics MQTT

```
tvm/{tvm_id}/etat
tvm/{tvm_id}/occupation                     # retained : {occupe: bool, compte_id: str|null}
tvm/{tvm_id}/vente
tvm/{tvm_id}/caisse                          # niveau détaillé par dénomination
tvm/{tvm_id}/erreurs/{type_erreur}
tvm/{tvm_id}/heartbeat
tvm/{tvm_id}/commands/selection_titre        # client → TVM
tvm/{tvm_id}/commands/inserer_espece         # client → TVM
tvm/{tvm_id}/commands/payer_carte            # client → TVM
tvm/{tvm_id}/commands/annuler                # client → TVM
tvm/{tvm_id}/paiement_compte/demande         # TVM → Collector
tvm/{tvm_id}/paiement_compte/reponse         # Collector → TVM
tvm/{tvm_id}/commands/admin/approvisionner   # admin-dashboard → Collector → TVM
alerts/{type_alerte}/{tvm_id}
```

| Topic | Publié par | QoS | Retained |
|---|---|---|---|
| `tvm/{id}/etat` | TVM | 1 | Oui |
| `tvm/{id}/occupation` | Collector | 1 | Oui |
| `tvm/{id}/vente` | TVM | 2 | Non |
| `tvm/{id}/caisse` | TVM | 1 | Oui |
| `tvm/{id}/erreurs/{type}` | TVM | 1 | Non |
| `tvm/{id}/heartbeat` | TVM | 0 | Oui |
| `tvm/{id}/commands/*` | Client-app | 1 | Non |
| `tvm/{id}/paiement_compte/*` | TVM / Collector | 1 | Non |
| `alerts/{type}/{id}` | Collector | 1 | Oui |

**LWT** : identique à la v1 — `tvm/{id}/etat` → `{"etat":"hors_ligne"}`, retained, QoS 1, publié automatiquement par le broker à toute déconnexion inattendue. À la détection d'un passage `hors_ligne`, le Collector force aussi la libération de la réservation en cours s'il y en avait une (un TVM déconnecté ne peut pas rester "occupé" indéfiniment aux yeux des autres clients).

---

## 10. Formats de payload (JSON) — extraits clés

### 10.1 `tvm/{id}/occupation`
```json
{ "tvm_id": "tvm_01", "occupe": true, "compte_id": "cl_92ab", "timestamp": 1755680400 }
```

### 10.2 `tvm/{id}/caisse`
```json
{
  "tvm_id": "tvm_01",
  "stock": { "0.10": 48, "0.20": 30, "0.50": 20, "1": 20, "2": 18, "5": 10, "10": 8, "20": 5, "50": 2 },
  "total": 312.30,
  "timestamp": 1755680400
}
```

### 10.3 `tvm/{id}/commands/inserer_espece`
```json
{ "compte_id": "cl_92ab", "denomination": 2, "timestamp": 1755680400 }
```

### 10.4 `tvm/{id}/commands/selection_titre`
```json
{
  "compte_id": "cl_92ab",
  "type_titre": "carnet",
  "zone": "A-B",
  "quantite": 1,
  "timestamp": 1755680400
}
```

### 10.5 `tvm/{id}/vente`
```json
{
  "tvm_id": "tvm_01",
  "compte_id": "cl_92ab",
  "transaction_id": "a1b2c3d4",
  "type_titre": "carnet",
  "zone": "A-B",
  "quantite": 1,
  "prix_total": 14.50,
  "mode_paiement": "especes",
  "timestamp": 1755680400
}
```

*(Les payloads `etat`, `erreurs`, `heartbeat`, `alerts`, `paiement_compte/demande-reponse` restent structurellement identiques à la v1 — seul `mode_paiement` est désormais strictement `especes` ou `carte`.)*

---

## 11. Architecture logicielle détaillée

### 11.1 TVM simulé — modules internes
- **`TvmSupervisor`** : machine à états pure, pilotée uniquement par des événements réels (commandes MQTT reçues, réponses des sous-systèmes) — **aucun générateur aléatoire de transactions**
- **`CashDrawer`** (caisse) : stock de dénominations en mémoire + persistance via le Collector, applique `calculer_rendu()`, gère l'insertion et le rollback en cas d'échec de rendu
- **`CardReader`** : envoie la demande de débit au Collector et attend la réponse corrélée avec timeout
- **`PrinterUnit`** : impression avec risque simulé de bourrage (probabilité fixe, fault-injection matérielle uniquement)
- **`HeartbeatPublisher`** : identique à la v1

### 11.2 Collector — modules internes
- **`AuthManager`** : inscription, connexion (hash salé + vérification), gestion des sessions (jeton + expiration)
- **`ReservationManager`** : réservation/libération atomique d'un TVM (`BEGIN IMMEDIATE` SQLite), thread de libération automatique par timeout d'inactivité
- **`CashManager`** : approvisionnement, encaissement, rendu de monnaie — miroir côté Collector du stock de chaque TVM (source de vérité persistée)
- **`ComptesManager`** : débit/recharge de solde utilisateur, atomique (repris de la v1)
- **`MqttListener` + pool de workers + `StorageWriter`** : identique à la v1 (SafeQueue, callback non bloquant, écriture SQLite dédiée en mode WAL)
- **`RestApiServer`** : expose l'ensemble des endpoints de la section 11.4, avec vérification du rôle (`client`/`admin`) sur chaque route sensible

### 11.3 Schéma de base SQLite (complet)

```sql
CREATE TABLE comptes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    compte_id TEXT UNIQUE NOT NULL,
    nom TEXT NOT NULL,
    email TEXT UNIQUE NOT NULL,
    mot_de_passe_hash TEXT NOT NULL,
    sel TEXT NOT NULL,
    role TEXT NOT NULL DEFAULT 'client',
    solde REAL NOT NULL DEFAULT 0,
    created_at INTEGER NOT NULL,
    derniere_connexion INTEGER
);

CREATE TABLE sessions (
    token TEXT PRIMARY KEY,
    compte_id TEXT NOT NULL,
    created_at INTEGER NOT NULL,
    expire_at INTEGER NOT NULL
);

CREATE TABLE mouvements_solde (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    compte_id TEXT NOT NULL,
    type TEXT NOT NULL,             -- creation | recharge_client | recharge_admin | debit_achat
    montant REAL NOT NULL,
    solde_apres REAL NOT NULL,
    transaction_id TEXT,
    admin_id TEXT,
    timestamp INTEGER NOT NULL
);

CREATE TABLE tvm_registry (
    tvm_id TEXT PRIMARY KEY,
    nom TEXT NOT NULL,
    etat TEXT NOT NULL DEFAULT 'IDLE',
    occupe_par TEXT,
    derniere_activite INTEGER,
    derniere_connexion INTEGER
);

CREATE TABLE tvm_caisse (
    tvm_id TEXT NOT NULL,
    denomination REAL NOT NULL,
    quantite INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (tvm_id, denomination)
);

CREATE TABLE mouvements_caisse (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tvm_id TEXT NOT NULL,
    denomination REAL NOT NULL,
    delta INTEGER NOT NULL,          -- positif = reçu, négatif = rendu/retiré
    type TEXT NOT NULL,              -- approvisionnement | encaissement | rendu_monnaie
    transaction_id TEXT,
    timestamp INTEGER NOT NULL
);

CREATE TABLE ventes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tvm_id TEXT NOT NULL,
    compte_id TEXT NOT NULL,
    transaction_id TEXT NOT NULL,
    type_titre TEXT NOT NULL,
    zone TEXT,
    quantite INTEGER NOT NULL,
    prix_total REAL NOT NULL,
    mode_paiement TEXT NOT NULL,     -- especes | carte
    timestamp INTEGER NOT NULL
);

CREATE TABLE erreurs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tvm_id TEXT NOT NULL,
    type_erreur TEXT NOT NULL,
    message TEXT,
    transaction_id TEXT,
    timestamp INTEGER NOT NULL
);

CREATE TABLE alertes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tvm_id TEXT NOT NULL,
    type_alerte TEXT NOT NULL,
    severite TEXT NOT NULL,
    message TEXT,
    timestamp INTEGER NOT NULL
);

CREATE INDEX idx_ventes_tvm ON ventes(tvm_id);
CREATE INDEX idx_mouvements_compte ON mouvements_solde(compte_id);
CREATE INDEX idx_mouvements_caisse_tvm ON mouvements_caisse(tvm_id);
```

### 11.4 Endpoints API REST (complet)

| Méthode | Endpoint | Rôle requis | Description |
|---|---|---|---|
| POST | `/api/auth/register` | public | Crée un compte client, solde initial aléatoire |
| POST | `/api/auth/login` | public | Retourne un jeton de session |
| POST | `/api/auth/logout` | client/admin | Invalide le jeton de session |
| GET | `/api/tvm` | client/admin | Liste des TVM avec disponibilité et état |
| POST | `/api/tvm/{id}/reserver` | client/admin | Réservation atomique |
| POST | `/api/tvm/{id}/liberer` | client/admin | Libération (doit correspondre au réservataire) |
| GET | `/api/comptes/{id}` | propriétaire/admin | Solde + historique |
| POST | `/api/comptes/{id}/recharger` | propriétaire | Auto-recharge |
| GET | `/api/admin/dashboard` | admin | Vue agrégée TVM + comptes |
| POST | `/api/admin/tvm/{id}/approvisionner` | admin | Ajoute des espèces à la caisse d'un TVM |
| POST | `/api/admin/comptes/{id}/recharger` | admin | Recharge le solde de n'importe quel compte |
| POST | `/api/admin/tvm/{id}/forcer_liberation` | admin | Libère un TVM bloqué (cas exceptionnel) |

---

## 12. Frontend Vue.js — spécification

### 12.1 Application cliente (`client-app`)
- **Écran de connexion/inscription**
- **Vue Flotte** : grille de TVM avec disponibilité en temps réel (`libre` en vert, `occupe` en gris avec mention "en cours d'utilisation"), bouton "Utiliser ce TVM" actif uniquement si `libre`
- **Écran d'interaction TVM** (une fois réservé) : sélection de titre, choix du moyen de paiement, boutons de dénominations pour l'insertion d'espèces (ou écran carte avec solde affiché en direct), suivi d'état en temps réel via MQTT
- **Écran de compte** : solde, bouton recharge, historique des mouvements

```js
// Vue Flotte — disponibilité en temps réel
client.subscribe('tvm/+/occupation')
client.subscribe('tvm/+/etat')

// Réservation
await fetch(`/api/tvm/${tvmId}/reserver`, {
  method: 'POST',
  headers: { Authorization: `Bearer ${token}` }
})

// Insertion d'espèces
client.publish(`tvm/${tvmId}/commands/inserer_espece`, JSON.stringify({
  compte_id: compteId, denomination: 2, timestamp: Math.floor(Date.now()/1000)
}))
```

### 12.2 Dashboard admin (`admin-dashboard`)
- **Tableau TVM** : état, disponibilité + utilisateur actuel, niveau de caisse (jauge par dénomination), bouton "Approvisionner"
- **Tableau comptes** : solde, dernière connexion, bouton "Recharger"
- Abonnement large `tvm/#` pour une vue temps réel complète

---

## 13. Subtilités et pièges à anticiper

1. **Réservation atomique obligatoire** : deux clients cliquant "Utiliser ce TVM" à la même milliseconde ne doivent jamais réussir tous les deux. Utiliser `BEGIN IMMEDIATE` en SQLite pour la mise à jour de `occupe_par`, exactement comme pour le débit de solde (section 6.2) — même famille de problème, même solution technique.

2. **Rollback du rendu de monnaie échoué** : si l'insertion a déjà été comptabilisée dans le stock du TVM (section 5.2) mais que le rendu de monnaie échoue, il faut annuler proprement l'ajout — sinon le client perd son argent inséré sans obtenir ni titre ni monnaie. Traiter insertion + validation comme une seule transaction logique avec possibilité de rollback complet.

3. **Libération automatique par timeout** : sans ce mécanisme, un client qui ferme son onglet sans cliquer "Quitter" bloque définitivement un TVM pour tout le monde. Le thread de nettoyage doit tourner en continu côté Collector (ex: vérification toutes les 30s des `derniere_activite` de plus de 3 minutes).

4. **TVM déconnecté alors qu'il était `occupe`** : à la réception du LWT (`hors_ligne`), le Collector doit aussi forcer `occupe_par = NULL` — sinon un TVM planté reste éternellement "en cours d'utilisation" aux yeux des autres clients alors qu'il est en réalité hors service.

5. **Authentification simplifiée** (voir section 1.5) : le hash SHA-256 salé protège contre le stockage en clair mais n'a pas la robustesse d'un `bcrypt`/`argon2` (pas de facteur de coût ajustable) — acceptable pour un projet personnel non exposé publiquement, à ne jamais réutiliser tel quel pour un vrai produit.

6. **Autorisation des commandes MQTT non vérifiée par le broker** : Mosquitto, dans la configuration de ce projet, n'a pas d'ACL par client — n'importe qui connecté au broker pourrait publier `tvm/{id}/commands/*` avec un `compte_id` arbitraire. Le Collector doit donc **revalider côté serveur** que le `compte_id` reçu correspond bien à une session active et à la réservation en cours du TVM concerné, en défense en profondeur, avant de traiter la commande.

7. **QoS 2 sur `vente` uniquement**, QoS 1 sur `etat`/`occupation`/`caisse`, QoS 0 sur `heartbeat` — mêmes justifications qu'en v1.

8. **Callback Paho ne doit jamais bloquer** — toujours pousser en SafeQueue, jamais parser/écrire directement dedans.

9. **`clean_start(false)` + `CLIENT_ID` stable et unique** par TVM pour survivre aux courtes coupures réseau sans perdre les commandes en attente.

10. **Le `#` doit être précédé d'un `/`** — toujours vérifier une souscription avant de déboguer un silence radio.

---

## 14. Plan de développement indicatif

Le scope ayant significativement grandi par rapport à une v1 "flotte autonome", ce planning dépasse un simple weekend de 16h — il est découpé en phases pour permettre un développement incrémental, chaque phase étant démontrable indépendamment.

### Phase A — Socle (≈ 10h)
- Setup Mosquitto, CMake, dépendances
- `TvmSupervisor` piloté par événements MQTT explicites (un seul TVM, sans caisse ni compte pour commencer — juste la machine à états)
- `AuthManager` basique : inscription/connexion, sessions

### Phase B — Argent réel (≈ 8h)
- `CashDrawer` + `calculer_rendu()` + rollback en cas d'échec
- `ComptesManager` + flux requête/réponse paiement carte
- Persistance complète (toutes les tables de la section 11.3)

### Phase C — Flotte et réservation (≈ 6h)
- Déploiement de 10+ instances de TVM
- `ReservationManager` (réservation atomique + libération auto par timeout)
- Vue Flotte côté client-app

### Phase D — Administration (≈ 4h)
- Endpoints admin (approvisionnement, recharge, forçage de libération)
- Dashboard admin complet

**Total estimé : ≈ 28h**, contre 16h pour la v1 initiale — cohérent avec l'ajout de l'authentification, de la caisse réelle par machine, et de la réservation multi-utilisateurs.

---

## 15. Fonctionnalités "nice-to-have"

1. Historique graphique (Chart.js) des ventes par TVM et par compte
2. Notification (son/toast) côté client-app quand le TVM réservé revient en erreur
3. Mode "file d'attente" : si tous les TVM sont occupés, proposer au client d'être notifié dès qu'un se libère
4. Export CSV des mouvements de caisse pour un TVM donné (audit)
5. `ThreadSanitizer` en CI

---

## 16. Critères d'acceptation (definition of done)

- [ ] Deux clients ne peuvent jamais réserver le même TVM simultanément (testé par un script envoyant deux requêtes concurrentes)
- [ ] Un paiement espèces avec rendu de monnaie impossible restitue intégralement les espèces insérées, sans perte pour le client ni incohérence de caisse
- [ ] Un paiement carte avec solde insuffisant est refusé et ne modifie pas le solde
- [ ] Un TVM abandonné sans clic "Quitter" est automatiquement libéré après le délai configuré
- [ ] Le dashboard admin reflète en temps réel l'état de chaque TVM (disponibilité, caisse) et de chaque compte (solde)
- [ ] Un admin peut approvisionner un TVM et recharger un compte, chaque opération étant journalisée
- [ ] Aucun deadlock ni thread zombie après un arrêt propre de chaque composant
- [ ] Le code compile sans warning avec `-Wall -Wextra`

---

## 17. Glossaire

| Terme | Définition |
|---|---|
| TVM | Ticket Vending Machine — distributeur automatique de titres de transport |
| QoS | Quality of Service — niveau de garantie de livraison MQTT (0, 1 ou 2) |
| LWT | Last Will and Testament — message publié automatiquement par le broker si un client se déconnecte anormalement |
| Réservation | Verrouillage exclusif d'un TVM par un compte utilisateur, empêchant tout autre client de l'utiliser en même temps |
| Rendu de monnaie | Calcul de la différence entre montant inséré et prix, composée à partir du stock de dénominations disponibles dans la caisse du TVM |
| Grand livre (ledger) | Table append-only journalisant chaque mouvement (solde ou caisse) plutôt que de ne conserver qu'une valeur courante mutable |
| Session | Jeton temporaire associé à un compte, généré à la connexion, requis pour les appels API authentifiés |

---

## 18. Stratégie de tests unitaires (gtest)

### 18.1 Composants à tester, par priorité

| Priorité | Composant | Ce qui est testé |
|---|---|---|
| 1 | Machine à états (`TvmSupervisor`) | Transitions valides/invalides |
| 2 | `calculer_rendu()` | Rendu correct, échec explicite si stock insuffisant, choix glouton optimal |
| 3 | `ReservationManager` | Réservation atomique sous concurrence, libération, timeout |
| 4 | `ComptesManager` | Débit/recharge atomiques, refus solde insuffisant |
| 5 | `AuthManager` | Hash/vérification de mot de passe, expiration de session |
| 6 | Payloads JSON | Round-trip, champs manquants explicites |

### 18.2 Exemples clés

```cpp
TEST(CalculRendu, RenduExactAvecGrossesCoupuresDabord) {
    std::map<double,int> stock = {{5,2},{2,3},{1,5},{0.5,10}};
    auto rendu = calculer_rendu(7.5, stock);
    ASSERT_TRUE(rendu.has_value());
    EXPECT_EQ((*rendu)[5], 1);
    EXPECT_EQ((*rendu)[2], 1);
    EXPECT_EQ((*rendu)[0.5], 1);
}

TEST(CalculRendu, EchecExplicteSiStockInsuffisant) {
    std::map<double,int> stock = {{50,1}};   // pas de petites coupures
    auto rendu = calculer_rendu(3.0, stock);
    EXPECT_FALSE(rendu.has_value());
}

TEST(ReservationManager, DeuxReservationsConcurrentesUneSeuleReussit) {
    ReservationManager mgr(":memory:");
    mgr.enregistrer_tvm("tvm_01");

    std::atomic<int> succes{0};
    auto tache = [&](const std::string& compte) {
        if (mgr.reserver("tvm_01", compte)) succes++;
    };
    std::thread t1(tache, "cl_a"), t2(tache, "cl_b");
    t1.join(); t2.join();

    EXPECT_EQ(succes.load(), 1);
}

TEST(ReservationManager, LiberationAutomatiqueApresTimeout) {
    ReservationManager mgr(":memory:");
    mgr.enregistrer_tvm("tvm_01");
    mgr.reserver("tvm_01", "cl_a");
    mgr.forcer_derniere_activite("tvm_01", /* il y a 5 minutes */ -300);

    mgr.verifier_timeouts(/* seuil */ 180);

    EXPECT_FALSE(mgr.est_occupe("tvm_01"));
}

TEST(ComptesManager, DebitEchoueSiSoldeInsuffisantEtSoldeInchange) {
    ComptesManager mgr(":memory:");
    std::string id = mgr.creer_compte("Test", "t@t.fr", "motdepasse");
    mgr.forcer_solde(id, 5.0);

    auto r = mgr.debiter(id, 10.0, "tx_001");

    EXPECT_EQ(r.statut, "refuse_solde_insuffisant");
    EXPECT_DOUBLE_EQ(mgr.get_solde(id), 5.0);
}
```

---

## 19. Arborescence de projet proposée

```
tvm-platform/
├── CMakeLists.txt
├── common/
│   ├── SafeQueue.hpp
│   └── json_helpers.hpp
├── tvm-simulator/
│   ├── main.cpp
│   ├── TvmSupervisor.hpp / .cpp
│   ├── CashDrawer.hpp / .cpp
│   ├── CardReader.hpp / .cpp
│   ├── PrinterUnit.hpp / .cpp
│   └── HeartbeatPublisher.hpp / .cpp
├── collector/
│   ├── main.cpp
│   ├── MqttListener.hpp / .cpp
│   ├── AuthManager.hpp / .cpp
│   ├── ReservationManager.hpp / .cpp
│   ├── CashManager.hpp / .cpp
│   ├── ComptesManager.hpp / .cpp
│   ├── StorageWriter.hpp / .cpp
│   └── RestApiServer.hpp / .cpp
├── tests/
│   ├── test_supervisor.cpp
│   ├── test_calcul_rendu.cpp
│   ├── test_reservation.cpp
│   ├── test_comptes.cpp
│   ├── test_auth.cpp
│   └── test_payload.cpp
├── client-app/
│   ├── package.json
│   ├── src/
│   │   ├── App.vue
│   │   ├── components/
│   │   │   ├── Login.vue
│   │   │   ├── FlotteView.vue
│   │   │   ├── TvmInteraction.vue
│   │   │   ├── PaiementEspeces.vue
│   │   │   ├── PaiementCarte.vue
│   │   │   └── CompteView.vue
│   │   └── stores/
├── admin-dashboard/
│   ├── package.json
│   ├── src/
│   │   ├── App.vue
│   │   └── components/
│   │       ├── TvmTable.vue
│   │       ├── ComptesTable.vue
│   │       └── ApprovisionnementModal.vue
└── mosquitto/
    └── websockets.conf
```
