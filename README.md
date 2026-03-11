# Final Assignment - **Integrated Healthcare Timetabling Problem (IHTP)**

## Cos'è il problema

L'**Integrated Healthcare Timetabling Problem (IHTP)** è un problema di ottimizzazione combinatoria che integra tre sotto-problemi ospedalieri:

1. **PAS** – Patient Admission Scheduling (quando e dove ricoverare i pazienti)
2. **NRA** – Nurse-to-Room Assignment (quale infermiere si occupa di quale stanza per ogni turno)
3. **SCP** – Surgical Case Planning (in quale sala operatoria si opera ogni paziente)

L'obiettivo è **minimizzare una funzione di costo** che penalizza la violazione dei vincoli soft, rispettando sempre tutti i vincoli hard.

---

## L'orizzonte temporale

- Il periodo di pianificazione dura **D giorni** (multiplo di 7: 14, 21 o 28 giorni)
- Ogni giorno ha **3 turni**: early (mattina), late (pomeriggio), night (notte)
- In totale ci sono quindi **3D turni**, numerati da 0 a 3D−1

---

## Le entità coinvolte

**Risorse infrastrutturali:**

- **Sale operatorie (OT)**: hanno una capacità massima giornaliera in minuti; alcune possono essere non disponibili certi giorni
- **Stanze**: hanno un numero di letti (capacità); alcune stanze sono incompatibili con certi pazienti

**Risorse umane:**

- **Infermieri**: hanno un livello di competenza (da 0 a L−1), un roster fisso di turni in cui lavorano, e un carico massimo per ogni turno
- **Chirurghi**: hanno un tempo massimo di chirurgia per ogni giorno (0 = non disponibile)

**Pazienti:**

- Possono essere **obbligatori** (devono essere ricoverati nel periodo) o **opzionali** (possono essere rimandati)
- Per ogni paziente sono noti:
    - Data di rilascio (prima data possibile di ammissione)
    - Data di scadenza (solo per obbligatori)
    - Gruppo d'età
    - Sesso
    - Durata del ricovero
    - Stanze incompatibili
    - Chirurgo assegnato
    - Durata dell'intervento
    - Per ogni turno del ricovero: carico di lavoro generato e livello minimo di competenza infermieristica richiesto
- **Occupanti**: pazienti già presenti all'inizio del periodo, con stanza e data di ammissione già fissate

---

## La soluzione da produrre

Per ogni istanza devi determinare 4 cose:

1. **Data di ammissione** di ogni paziente (o rimando per gli opzionali)
2. **Stanza** assegnata a ogni paziente ammesso (per tutta la durata del ricovero, senza trasferimenti)
3. **Infermiere** assegnato a ogni stanza occupata, per ogni turno
4. **Sala operatoria** assegnata a ogni paziente per il giorno dell'intervento (che coincide col giorno di ammissione)

---

## Vincoli Hard (H) — devono essere sempre rispettati

| Codice | Descrizione |
| --- | --- |
| H1 | Nessun mix di genere nella stessa stanza nello stesso giorno |
| H2 | Il paziente va in una stanza compatibile |
| H3 | Il chirurgo non può superare il suo tempo massimo giornaliero |
| H4 | La sala operatoria non può superare la sua capacità giornaliera |
| H5 | Tutti i pazienti obbligatori devono essere ammessi |
| H6 | Il paziente può essere ammesso solo tra la sua release date e due date |
| H7 | Il numero di pazienti in una stanza non supera la sua capacità |
| H8 | Ogni stanza occupata deve avere un infermiere in servizio assegnato |

---

## Vincoli Soft (S) — contribuiscono al costo da minimizzare

| Codice | Descrizione | Peso (esempio) |
| --- | --- | --- |
| S1 | Minimizzare la differenza di gruppi d'età nella stessa stanza | 5 |
| S2 | L'infermiere assegnato deve avere competenza sufficiente per i pazienti | 10 |
| S3 | Minimizzare il numero di infermieri distinti che si prendono cura di un paziente (continuità di cura) | 5 |
| S4 | L'infermiere non deve superare il suo carico massimo | 10 |
| S5 | Minimizzare il numero di sale operatorie aperte ogni giorno | 20 |
| S6 | Minimizzare il numero di sale diverse in cui opera uno stesso chirurgo nello stesso giorno | 1 |
| S7 | Minimizzare il ritardo di ammissione rispetto alla release date | 5 |
| S8 | Minimizzare il numero di pazienti opzionali non ammessi | 350 |

I pesi sono **specifici per ogni istanza** e sono contenuti nel file JSON di input.

---

## Il tuo obiettivo (come indicato dal professore)

Implementare una soluzione **greedy** — cioè costruttiva, senza ricerca locale o ottimizzazione avanzata. L'approccio greedy significa prendere decisioni una alla volta in modo "ragionevole" (es. prima i pazienti obbligatori, poi gli opzionali; assegnare prima le stanze più compatibili, ecc.), senza tornare indietro a migliorare. Il vantaggio è che potrai poi **confrontare oggettivamente** la qualità dei tuoi risultati con quelli dei finalisti della competizione, che usavano MILP, metaeuristiche, ecc.

Un possibile approccio greedy articolato in fasi:

1. **Ordina i pazienti** (es. prima i mandatori per urgenza, poi gli opzionali)
2. **Assegna data di ammissione e stanza** rispettando H1, H2, H6, H7
3. **Assegna la sala operatoria** rispettando H3, H4 (es. quella con più capacità residua)
4. **Assegna gli infermieri** alle stanze per ogni turno, rispettando H8 e cercando di minimizzare S2, S3, S4

---

## Materiale necessario

Tutto è disponibile sul sito della competizione: [**https://ihtc2024.github.io**](https://ihtc2024.github.io/)

Hai bisogno di:

- **Dataset pubblico**: 30 istanze (`i01`–`i30`) in formato JSON, più 10 istanze di test (`test01`–`test10`) già con soluzioni di esempio — queste ultime sono utilissime per testare il tuo codice
- **Validatore**: fornito come codice sorgente C++, da compilare con g++. Prende in input il file istanza e il file soluzione e restituisce sia le violazioni dei vincoli hard che il costo totale
- **Formato dei file**: descritto nell'Appendice A del paper — input e output sono entrambi in **JSON**

In sintesi, il flusso di lavoro sarà: scarichi un'istanza JSON → la dai in input al tuo algoritmo greedy → ottieni una soluzione JSON → la passi al validatore per vedere quante violazioni hai e qual è il costo → confronti il costo con gli upper bound riportati in Table 4 del paper.

---
