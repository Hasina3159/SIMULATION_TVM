# CardReader — Notes de conception (à lire avant d'implémenter)

## 1. Rôle (CDC section 6, 11.1)

`CardReader` gère le paiement par carte : débiter le compte du client actuellement connecté.
Le solde vit côté Collector, jamais côté TVM — le TVM ne débite jamais lui-même.

Flux : le TVM envoie une **demande** de débit, puis **attend une réponse corrélée**, avec timeout.
Si aucune réponse n'arrive dans le délai (ex 5s), c'est un **échec** — jamais un succès implicite.

## 2. Topics et payloads exacts (CDC section 6.1)

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

Valeurs possibles de `statut` : `accepte`, `refuse_solde_insuffisant`, `refuse_compte_inconnu`, `timeout`.

## 3. Le vrai défi technique

Contrairement à `HeartbeatPublisher` (un seul thread, publie dans le vide, personne n'attend de
réponse), ici :

- Le thread qui appelle `CardReader::pay(...)` publie la demande puis doit **se mettre en attente**.
- La réponse arrive plus tard, sur le **thread de callback interne de Paho** (`message_arrived`) —
  jamais sur le thread qui attend.

**Conséquence directe sur l'interface** : `IMqttPublisher` (utilisée par `HeartbeatPublisher`) ne
suffit plus, elle ne sait que *publier*. `CardReader` a aussi besoin de *recevoir* — il faut décider
comment le thread de callback Paho transmet un message entrant à `CardReader`.

Deux architectures possibles pour ça (à trancher avant de coder) :
- `CardReader` implémente lui-même l'interface `mqtt::callback` de Paho et s'enregistre pour le
  topic `paiement_compte/reponse`.
- Un dispatcher central du TVM (dans `main.cpp` ou une classe dédiée) reçoit tous les messages
  entrants et les redistribue aux composants intéressés (`CardReader` pour les réponses de
  paiement, plus tard d'autres composants pour d'autres topics).

## 4. Deux façons de faire "publier puis attendre une réponse corrélée"

### Option A — `std::promise` / `std::future` par requête
- À chaque appel : créer une paire promise/future, la stocker dans une map partagée
  `correlation_id → promise` (protégée par mutex), publier la demande, puis
  `future.wait_for(timeout)`.
- Quand la réponse arrive côté callback : retrouver la bonne promise via son `correlation_id`,
  `promise.set_value(reponse)`.
- Avantage : `std::future` est fait exactement pour ce cas ("valeur calculée ailleurs, disponible
  plus tard, avec timeout"), code très lisible côté appelant.

### Option B — `condition_variable` + map partagée
- Une map `correlation_id → std::optional<reponse>`, protégée par un `mutex`.
- Le thread qui attend : `cv.wait_for(lock, timeout, [&]{ return map[id].has_value(); })`.
- Le callback Paho : verrouille, remplit `map[id]`, `notify_all()`.
- Plus bas niveau, mais c'est l'exercice de concurrence "au-delà du simple
  producteur/consommateur" visé par le CDC (section 1.3).

### Piège commun aux deux options
Si le timeout expire **puis** que la réponse arrive quand même en retard : il faut nettoyer/retirer
l'entrée correspondante après résolution (succès, échec ou timeout), sinon soit elle traîne pour
toujours, soit une réponse tardive vient perturber un appel déjà terminé.

## 5. Interface publique suggérée (à ajuster)

```cpp
struct PaymentResult {
    bool success;
    std::string statut;        // "accepte" | "refuse_solde_insuffisant" | "refuse_compte_inconnu" | "timeout"
    double nouveau_solde;
};

class CardReader {
public:
    PaymentResult pay(const std::string &compte_id, double montant, const std::string &transaction_id);

    // Appelé par le dispatcher MQTT (ou directement par le callback Paho) quand une réponse arrive
    void on_response_received(const std::string &payload);
};
```

## 6. Comportement attendu (contrat)

- Génère un `correlation_id` unique par appel à `pay()`.
- Publie sur `tvm/{id}/paiement_compte/demande`.
- Bloque jusqu'à : réponse correspondante reçue **ou** timeout (5s par défaut, configurable pour
  les tests).
- Ne renvoie **jamais** un succès implicite en cas de timeout ou d'erreur de parsing.
- Nettoie l'entrée en attente après résolution, quel que soit le résultat.
- Une réponse malformée ou avec un `correlation_id` inconnu est ignorée silencieusement (ou
  loguée) — ne doit jamais faire planter le programme.
- Deux appels concurrents à `pay()` (deux `correlation_id` différents) ne doivent jamais
  s'interférer.

## 7. Tests à écrire (`tests/test_card_reader.cpp`)

1. Réponse reçue avant le timeout → `pay()` retourne succès avec le bon `nouveau_solde`.
2. Aucune réponse reçue → `pay()` retourne `timeout` après le délai configuré (pas avant, pas
   indéfiniment après).
3. Refus explicite (`refuse_solde_insuffisant`) → `pay()` retourne cet échec précis, jamais un
   succès.
4. Deux transactions concurrentes avec des `correlation_id` différents ne se mélangent pas
   (chaque appelant reçoit la bonne réponse, pas celle de l'autre).
5. Une réponse avec un `correlation_id` inconnu ou déjà résolu est ignorée sans crash.
6. (si testable) une réponse arrivant après qu'un timeout a déjà été consommé ne casse rien et
   n'affecte pas un appel ultérieur avec le même `correlation_id` réutilisé par hasard.

## 8. Points à trancher avant de coder

- [ ] Option A (promise/future) ou Option B (condition_variable + map) ?
- [ ] Qui reçoit les messages MQTT entrants : `CardReader` lui-même via `mqtt::callback`, ou un
      dispatcher central du TVM ?
- [ ] Le timeout doit-il être un paramètre du constructeur (pour être raccourci dans les tests),
      ou une constante fixe ?
