# TASK: Ottimizzazione Hot Paths tramite Interning (Rimozione Stringhe)

**Ruolo:** Agisci come un Senior C++ Software Engineer esperto di ottimizzazione delle performance.

**Contesto del problema:**
Nei file `IHTC_Data.hh` e `IHTC_Data.cc`, alcune proprietà fondamentali (`gender`, `incompatible_rooms`, `surgeon_id`, `age_group`) sono modellate come `std::string`. Durante la fase di assegnazione (`canAssignPatient`, `assignPatient`) e calcolo dei costi, il codice esegue milioni di confronti tra stringhe e ricerche testuali. Questo distrugge le performance della CPU (confronto O(K) invece di O(1)). L'ID del paziente (`p01`, etc.) è già correttamente indicizzato, ma gli altri campi no.

**Obiettivo:**
Sostituire le stringhe con identificatori numerici interi (`int`) o enumeratori (`enum class`) all'interno delle strutture dati e nei percorsi critici di calcolo. La traduzione da stringa a numero avverrà una sola volta durante il parsing del JSON.

Esegui i seguenti step in ordine:

### STEP 1: Modifica `IHTC_Data.hh`
1. Apri `IHTC_Data.hh`.
2. Aggiungi questo `enum` in cima al file (dopo gli `#include` e prima delle struct):
```cpp
enum class Gender : int8_t {
    NONE = 0,
    A = 1,
    B = 2
};
```
3. Aggiorna la struct `Patient`:
   - Cambia `std::string sex;` in `Gender sex = Gender::NONE;`
   - Cambia `std::vector<std::string> incompatible_rooms;` in `std::vector<int> incompatible_room_idxs;`
   - Cambia `std::string surgeon_id;` in `int surgeon_idx = -1;`
   - Assicurati che `int age_group = -1;` sia presente (dovrebbe già esserci).
4. Aggiorna la struct `Occupant`:
   - Cambia `std::string sex;` in `Gender sex = Gender::NONE;`
   - Cambia `std::string room_id;` in `int room_idx = -1;`
5. Aggiorna la classe `IHTC_Output`:
   - Trova la dichiarazione: `std::vector<std::vector<std::string>> room_gender;`
   - Cambiala in: `std::vector<std::vector<Gender>> room_gender;`
   - Aggiorna la firma di `seedOccupantStay`:
     `void seedOccupantStay(int room_idx, int admission_day, int length_of_stay, Gender sex);`

### STEP 2: Modifica l'Inizializzazione in `IHTC_Data.cc`
1. Apri `IHTC_Data.cc`.
2. Nella funzione `IHTC_Output::init`, aggiorna l'inizializzazione di `room_gender`:
```cpp
    // Sostituisci la vecchia riga con questa:
    room_gender.assign(in.rooms.size(), std::vector<Gender>(days, Gender::NONE));
```

### STEP 3: Modifica i Percorsi Critici in `IHTC_Data.cc`
1. Sempre in `IHTC_Data.cc`, aggiorna `canAssignPatient`. Modifica i blocchi per Sesso, Stanze Incompatibili e Chirurgo per usare interi:
```cpp
    // 2. Vincoli della Stanza (Sesso) - SOSTITUISCI IL BLOCCO ESISTENTE
    for (int dd = 0; dd < los; ++dd) {
        int d_idx = day + dd;
        if (d_idx < 0 || d_idx >= days) break;
        if (room_occupancy[room_idx][d_idx] >= r.capacity) return false;
        
        // Nuovo controllo genere ultra-veloce (numerico)
        if (p.sex != Gender::NONE) {
            Gender g = room_gender[room_idx][d_idx];
            if (g != Gender::NONE && g != p.sex) return false;
        }
    }

    // 3. Stanze vietate - SOSTITUISCI IL BLOCCO ESISTENTE
    for (int bad_idx : p.incompatible_room_idxs) {
        if (bad_idx == room_idx) return false;
    }

    // 5. Vincoli del Chirurgo - SOSTITUISCI IL BLOCCO ESISTENTE
    if (p.surgeon_idx >= 0 && p.surgeon_idx < (int)surgeon_availability.size()) {
        if (day >= (int)surgeon_availability[p.surgeon_idx].size() || 
            surgeon_availability[p.surgeon_idx][day] < p.surgery_time) {
            return false;
        }
    }
```
2. Aggiorna `assignPatient`:
```cpp
    // Nel ciclo "Aggiornamento Stanza (Sesso)" sostituisci il check con:
    if (in.patients[patient_id].sex != Gender::NONE && room_gender[room_idx][dd_idx] == Gender::NONE) {
        room_gender[room_idx][dd_idx] = in.patients[patient_id].sex;
    }

    // Nel blocco "Aggiornamento Chirurgo" sostituisci con:
    if (in.patients[patient_id].surgeon_idx >= 0) {
        int surgeon_idx = in.patients[patient_id].surgeon_idx;
        if (surgeon_idx < (int)surgeon_availability.size() && day >= 0 && day < days) {
            surgeon_availability[surgeon_idx][day] -= in.patients[patient_id].surgery_time;
        }
    }
```
3. Aggiorna `seedOccupantStay`:
```cpp
    // Cambia il parametro `const std::string &sex` in `Gender sex` e aggiorna l'assegnazione:
    if (sex != Gender::NONE && room_gender[room_idx][d] == Gender::NONE) {
        room_gender[room_idx][d] = sex;
    }
```
4. Aggiorna il calcolo di `SurgeonTransfer` in `computeAllCosts` (o dove risiede):
```cpp
    // Rimuovi l'uso delle stringhe come chiavi. Usa p.surgeon_idx.
    // std::unordered_map<int, std::vector<std::set<int>>> surgeon_ot_by_day;
    // La logica sarà identica, ma userà 'int' invece di 'std::string'.
```

### STEP 4: Traduzione nel Parser JSON (`json/parser.cc` o simile)
1. Cerca il file responsabile del popolamento di `IHTC_Input` (`jsonio::load_instance`).
2. Implementa l'interning direttamente in fase di lettura:
   - **Gender**: Se leggi "A", imposta `p.sex = Gender::A;`. Se leggi "B", imposta `p.sex = Gender::B;`.
   - **Stanze Incompatibili**: Mappa l'array di stringhe JSON negli indici interi corrispondenti usando una mappa o ricercandoli in `in.rooms`. Salva gli indici in `p.incompatible_room_idxs`.
   - **Chirurgo**: Trova l'indice del chirurgo nell'array `in.surgeons` basandoti sulla stringa letta e salvalo in `p.surgeon_idx`.
   - **Occupanti**: Risolvi l'indice della stanza (`f.room_id` -> `f.room_idx`) e il sesso.

Assicurati che tutto compili. Questa modifica convertirà la validazione in O(1) puro!