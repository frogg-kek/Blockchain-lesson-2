````markdown
# Supaprastinta blokų grandinė — v0.1

Projektas yra edukacinė, supaprastinta blokų grandinės (blockchain) implementacija C++ kalba.
Tikslas — parodyti pagrindines koncepcijas: blokai, antraštės, transakcijos, (labai paprastas) PoW kasybas ir vartotojų balansų atnaujinimą.

## Kas realizuota
- Centralizuota grandinė su genesis bloku.
- Vartotojų ir transakcijų generavimas (konfigūruojamas kiekis).
- Transakcijos apdorojamos bloko pridedant: jei siuntėjas turi pakankamai, suma pervedama gavėjui.
- PoW kasimas: hash(header) turi prasidėti nuo tam tikro nulinių simbolių skaičiaus (difficulty).
- `transactions_hash` yra paprastas visų tx.id sujungimo hash (Merkle medis nėra naudojamas — tai paprastesnė versija).
- Konsolėje matoma kasimo eiga, nonce, galutinis hash, transakcijų taikymas [APPLIED]/[SKIPPED], grandinės aukštis ir likusios laukiamos transakcijos.
- OOP struktūra: `User`, `Transaction`, `BlockHeader`, `Block`, `Blockchain`.

## Kaip sukompiliuoti ir paleisti
1. Sukompiliuokite:

```bash
g++ -std=c++17 -O2 main.cpp -o blockchain
```

2. Paleiskite programą (numatytos reikšmės yra pradinės, jas galima keisti per CLI flag'us):

```bash
./blockchain
# arba greitas testas:
./blockchain --difficulty 2 --max-blocks 3
```

## CLI parinktys
- `--difficulty <n>` — kiek nulinių simbolių turi prasidėti hash'e (pvz. 2 arba 3).
- `--users <n>` — kiek vartotojų sugeneruoti.
- `--tx <n>` — kiek transakcijų sugeneruoti.
- `--tpb <n>` — transakcijų vienam blokui (tx per block).
- `--max-blocks <n>` — maksimalus iškasinėtų blokų skaičius; `-1` reiškia iki kol neliks transakcijų.

Pavyzdys su custom parametrais:

```bash
./blockchain --difficulty 2 --users 100 --tx 500 --tpb 50 --max-blocks 3
```

## Trumpi techniniai pastabos
- `Transaction` objektai turi konstruktorių, kuris užpildo laukus ir sugeneruoja `id` naudojant `HashFunkcija`.
- `Block::computeTransactionsHash` tiesiog sujungia visų transakcijų `id` reikšmes ir hashuoja rezultatą — tai yra paprasta alternatyva Merkle medžiui.
- Kasybos ciklas didina `nonce` kol header'o hash'as atitinka difficulty reikalavimą.

## Idėjos ir plėtiniai (ką verta įgyvendinti toliau)
Žemiau — keli įdomūs ir mokomieji plėtiniai, kuriuos galite įgyvendinti, skirstant pagal sudėtingumą:

- Lengvi / mokomieji:
  - Pridėti komandą, kuri išsaugo / pakrauna grandinę (pvz. JSON formatu).
  - Išplėsti transakcijų logiką su paprastu mokesčiu (fee) ir prioritetiniu pasirinkimu bloko sudėčiai.
  - Pakeisti `transactions_hash` į Merkle medį — tai parodo, kaip efektyviai tikrinti tx įtraukimą.

- Vidutinio sudėtingumo:
  - Implementuoti paprastą skaitmeninių parašų modelį (naudojant biblioteka arba stub funkcijas) — transakcijos turi pasirašytą siuntėjo raktą.
  - Pereiti nuo centrinės patikros prie paprastos P2P sinchronizacijos (TCP/UDP) tarp kelių nodų.
  - Pridėti bloko validacijos taisykles: pvz. neleidžiančias negatyvių sumų, duplikatų, arba viršijančių balanso transakcijų.

- Sudėtingesni / research:
  - Implementuoti UTXO modelį vietoje account-balance modelio (įvadas į Bitcoin panašią logiką).
  - Dinaminis difficulty: reguliuoti difficulty priklausomai nuo kasybos laiko (pvz. tikslinis bloko laikas).
  - Simuliuoti atakas (double-spend scenarijai) ir parodyti, kaip grandinė reaguoja.

---



````
