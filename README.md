
# Supaprastinta blokų grandinė — v0.1

## Kas realizuota
- Centralizuota grandinė su genesis bloku.
- Vartotojų (~1000) ir transakcijų (~10 000) generavimas.
- 100 tx/blokui, PoW: hash(header) turi prasidėti „000…“.
- `transactions_hash` = paprastas visų tx.id sujungimo hash (be Merkle).
- Konsolėje: kasimo eiga, nonce, hash, tx [APPLIED]/[SKIPPED], aukštis, likusios tx.
- OOP: `User`, `Transaction`, `BlockHeader`, `Block`, `Blockchain`.

## Paleidimas
```bash
g++ -std=c++17 -O2 main.cpp -o blockchain
./blockchain
# Greitam testui:
./blockchain --difficulty 2 --max-blocks 3

