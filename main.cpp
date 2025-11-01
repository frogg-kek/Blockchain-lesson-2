#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>

using namespace std;

#ifndef HASH_DYDIS
#define HASH_DYDIS 32
#endif

// HASH FUNKCIJA
std::string HashFunkcija(std::string tekstas){
    unsigned char hash[HASH_DYDIS] = {0};

    for(int i = 0; i<(int)tekstas.size(); i++){
        hash[i % HASH_DYDIS] ^= tekstas[i];
        
        if(tekstas[i] % 2 == 0){
            hash[i*3 % HASH_DYDIS] += 2;
        }
        if(tekstas[i] % 3 == 0){
            hash[i*2 % HASH_DYDIS] += 3;
        }
        else {
            hash[i*5 % HASH_DYDIS] += 4;
        }
        if(hash[i % HASH_DYDIS] > 128){
            hash[i % HASH_DYDIS] = 255 - hash[i % HASH_DYDIS];
        }
        if(i % 5 == 0){
            hash[(i*13) % HASH_DYDIS]+= (tekstas[i] % 2) + 1;
        }

        unsigned char temp = hash[i % HASH_DYDIS] >> 4;
        hash[i % HASH_DYDIS] = (hash[i % HASH_DYDIS] << 4) | temp;
    }

    for(int kartas = 0; kartas < 34; kartas++){
        for(int i = 0; i < HASH_DYDIS; i++){
            hash[i] ^= (hash[(i+7) % HASH_DYDIS] + hash[(i+13) % HASH_DYDIS]) ^ (i * 31);
            
            int pasukimai = (3 + kartas) % 8;
            hash[i] = (hash[i] << pasukimai) | (hash[i] >> (8 - pasukimai));
            hash[i] = (hash[i] * 31 + (kartas * 17)) & 255;
        }
    }
    
    char hexmap[] = "0123456789abcdef";
    std::string out;
    out.reserve(HASH_DYDIS);

    for(int i = 0; i < HASH_DYDIS; i++){
        unsigned char c = hash[i];
        out.push_back(hexmap[c >> 4]);
        out.push_back(hexmap[c & 15]);
    }
    return out; 
}

// PAGALBINES FUNKCIJOS REIKALINGOS 0 PATIKRINIMUI
static inline bool starts_with_zeros(const string& hexHash, unsigned zeros) {
    if (hexHash.size() < zeros) return false;
    for (unsigned i = 0; i < zeros; ++i) if (hexHash[i] != '0') return false;
    return true;
}

static inline uint64_t nowSec() {
    using namespace chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

// VARTOTOJO KLASĖ
class User {
public:

    User() = default;
    User(string name, string publicKey, long long balance) : name_(std::move(name)), publicKey_(std::move(publicKey)), balance_(balance) {}

    const string& getName() const { return name_; }
    const string& getPublic_key() const { return publicKey_; }
    long long getBalance() const { return balance_; }

    void deposit(long long amt) { balance_ += amt; }
    bool withdraw(long long amt) {
        if (amt > balance_) return false;
        balance_ -= amt; return true;
    }

private:
    string name_;
    string publicKey_;
    long long balance_{0};
};

// TRANSAKCIJOS KLASĖ
class Transaction {
public:

    Transaction() = default;

    Transaction(const string& sender, const string& receiver, long long amount, uint64_t timestamp) : sender_(sender), receiver_(receiver), amount_(amount), timestamp_(timestamp) {
        string toHash = sender_  + receiver_  + to_string(amount_)  + to_string(timestamp_);
        id_ = HashFunkcija(toHash);
    }

    const string& getId() const { return id_; }
    const string& getSender() const { return sender_; }
    const string& getReceiver() const { return receiver_; }
    long long getAmount() const { return amount_; }
    uint64_t getTimestamp() const { return timestamp_; }

private:
    string id_;
    string sender_;
    string receiver_;
    long long amount_{0};
    uint64_t timestamp_{0};
};

class UserManager {
public:
    void generateUsers(size_t nUsers) {
        uniform_int_distribution<long long> bal(100, 1'000'000);
        for (size_t i = 0; i < nUsers; ++i) {
            string name = randomName();
            string pk   = randomPublicKey();
            users_.emplace(pk, User{name, pk, bal(rng_)});
        }
        cout << "Sugeneruota vartotojų: " << users_.size() << "\n";
    }

    bool withdraw(const string& pk, long long amt) {
        auto it = users_.find(pk);
        return it != users_.end() && it->second.withdraw(amt);
    }
    void deposit(const string& pk, long long amt) {
        auto it = users_.find(pk);
        if (it != users_.end()) it->second.deposit(amt);
    }

    vector<string> keys() const {
        vector<string> out; out.reserve(users_.size());
        for (auto& kv : users_) out.push_back(kv.first);
        return out;
    }

    const unordered_map<string, User>& all() const { return users_; }

private:
    unordered_map<string, User> users_;
    mt19937_64 rng_{random_device{}()};

    string randomName() {
        static const char* syl[] = {"va","de","ra","li","no","ka","mi","to","sa","re","na","zo"};
        uniform_int_distribution<int> nSyl(2,3);
        uniform_int_distribution<int> pick(0, (int)(sizeof(syl)/sizeof(syl[0]))-1);
        string s; int k = nSyl(rng_); for (int i = 0; i < k; ++i) s += syl[pick(rng_)];
        s[0] = (char)toupper(s[0]); return s;
    }
    string randomPublicKey() {
        static const char* hex = "0123456789abcdef";
        uniform_int_distribution<int> d(0, 15);
        string s = "PUB"; for (int i = 0; i < 33; ++i) s += hex[d(rng_)]; return s;
    }
};

class TxPool {
public:
    void push(Transaction tx) { pending_.push_back(std::move(tx)); }
    vector<Transaction> take(size_t maxCount) {
        size_t takeN = std::min(maxCount, pending_.size());
        vector<Transaction> out; out.reserve(takeN);
        // paimam paskutinius takeN (greičiau nei iš vidurio)
        for (size_t i = 0; i < takeN; ++i) out.push_back(std::move(pending_[pending_.size()-1-i]));
        pending_.resize(pending_.size() - takeN);
        return out;
    }
    size_t size() const { return pending_.size(); }
private:
    vector<Transaction> pending_;
};

// Atskira funkcija transakcijoms generuoti (vietoj Blockchain::generateTransactions)
static void generateTransactions(TxPool& pool, const vector<string>& keys, size_t nTx, mt19937_64& rng) {
    if (keys.size() < 2) return;
    uniform_int_distribution<long long> amt(1, 5000);
    uniform_int_distribution<size_t> pick(0, keys.size()-1);

    for (size_t i = 0; i < nTx; ++i) {
        const string& sender = keys[pick(rng)];
        string receiver;
        do { receiver = keys[pick(rng)]; } while (receiver == sender);
        long long amount = amt(rng);
        pool.push(Transaction(sender, receiver, amount, nowSec()));
    }
    cout << " Sugeneruota laukiama transakciju: " << pool.size() << "\n";
}

// BLOKO ANTRAŠTĖS KLASĖ
class BlockHeader {
public:
    BlockHeader() = default;

    void set_prev_hash(string v) { prev_hash_ = std::move(v); }
    void set_timestamp(uint64_t v) { timestamp_ = v; }
    void set_version(string v) { version_ = std::move(v); }
    void set_transactions_hash(string v) { transactions_hash_ = std::move(v); }
    void set_nonce(uint64_t v) { nonce_ = v; }
    void set_difficulty(unsigned v) { difficulty_ = v; }

    const string& getPrev_hash() const { return prev_hash_; }
    uint64_t getTimestamp() const { return timestamp_; }
    const string& getVersion() const { return version_; }
    const string& getTransactions_hash() const { return transactions_hash_; }
    uint64_t getNonce() const { return nonce_; }
    unsigned getDifficulty() const { return difficulty_; }

    string to_string() const {
        return prev_hash_ + "|" + std::to_string(timestamp_) + "|" + version_ + "|" +
               transactions_hash_ + "|" + std::to_string(nonce_) + "|" + std::to_string(difficulty_);
    }

private:
    string prev_hash_;
    uint64_t timestamp_{0};
    string version_{"v2025"};
    string transactions_hash_;
    uint64_t nonce_{0};
    unsigned difficulty_{3};
};

// BLOKO KLASĖ
class Block {
public:
    Block() = default;

    BlockHeader& header() { return header_; }
    const BlockHeader& header() const { return header_; }

    const vector<Transaction>& transactions() const { return transactions_; }
    vector<Transaction>& transactions() { return transactions_; }

    const string& block_hash() const { return block_hash_; }
    void set_block_hash(string h) { block_hash_ = std::move(h); }

    static string computeTransactionsHash(const vector<Transaction>& txs) {
        // If no transactions, return hash of empty string (same behavior as before).
        if (txs.empty()) return HashFunkcija("");

        // Build leaves as transaction IDs (they are already hashes/ids)
        vector<string> layer; layer.reserve(txs.size());
        for (const auto& t : txs) layer.push_back(t.getId());

        // Iteratively compute parent layers until single root remains
        while (layer.size() > 1) {
            if (layer.size() % 2 == 1) {
                // duplicate last when odd number of nodes
                layer.push_back(layer.back());
            }
            vector<string> next; next.reserve(layer.size() / 2);
            for (size_t i = 0; i < layer.size(); i += 2) {
                // parent hash = HashFunkcija(left + right)
                next.push_back(HashFunkcija(layer[i] + layer[i+1]));
            }
            layer.swap(next);
        }

        return layer.front();
    }


private:
    BlockHeader header_;
    vector<Transaction> transactions_;
    string block_hash_;
};

// MINER KLASĖ
class Miner {
public:
    explicit Miner(unsigned difficulty) : difficulty_(difficulty) {}

    Block makeCandidate(const string& prevHash, unsigned difficulty, size_t txPerBlock, TxPool& pool) {
        Block b;
        b.header().set_prev_hash(prevHash);
        b.header().set_timestamp(nowSec());
        b.header().set_version("v2025");
        b.header().set_difficulty(difficulty);

        auto picked = pool.take(txPerBlock);
        b.transactions() = std::move(picked);

        b.header().set_transactions_hash(Block::computeTransactionsHash(b.transactions()));
        return b;
    }

    void mine(Block& block) {
        cout << " Kasam bloka: " << block.transactions().size()
             << " tx... tikslas: " << block.header().getDifficulty() << " nuliai pradzioje\n";

        auto start = chrono::high_resolution_clock::now();
        uint64_t nonce = 0;
        while (true) {
            block.header().set_nonce(nonce++);
            string h = HashFunkcija(block.header().to_string());
            if (starts_with_zeros(h, block.header().getDifficulty())) {
                block.set_block_hash(h);
                break;
            }
            if (nonce % 100000 == 0) {
                cout << "   ... bandyta nonce " << nonce << "\r" << flush;
            }
        }
        auto ms = chrono::duration_cast<chrono::milliseconds>(
                      chrono::high_resolution_clock::now() - start).count();
        cout << "\n Iskasta! nonce=" << block.header().getNonce()
             << " hash=" << block.block_hash() << " (" << ms << " ms)\n";
    }

private:
    unsigned difficulty_;
};

// BLOCKCHAIN KLASĖ
class Blockchain {
public:
    explicit Blockchain(unsigned difficulty, unsigned /*txPerBlock*/ = 100)
    : difficulty_(difficulty) {
        // Genesis
        Block genesis;
        genesis.header().set_prev_hash(string(HASH_DYDIS * 2, '0'));
        genesis.header().set_timestamp(nowSec());
        genesis.header().set_version("v0.1");
        genesis.header().set_difficulty(difficulty_);
        genesis.header().set_transactions_hash(Block::computeTransactionsHash(genesis.transactions()));
        genesis.set_block_hash(HashFunkcija(genesis.header().to_string()));
        chain_.push_back(std::move(genesis));
    }

    const Block& tip() const { return chain_.back(); }
    size_t height() const { return chain_.size() - 1; } // be genesis

    // Dabar addBlock priima UserManager, nes jis taiko balansus
    void addBlock(const Block& block, UserManager& users) {
        cout << " Pridedame bloka #" << chain_.size()
             << "  tx=" << block.transactions().size() << "\n";

        // Paprastos validacijos (realiai čia darytum daugiau)
        if (block.header().getPrev_hash() != tip().block_hash()) {
            cout << "   [SKIPPED] blogas prev_hash\n"; return;
        }
        if (!starts_with_zeros(block.block_hash(), block.header().getDifficulty())) {
            cout << "   [SKIPPED] netenkina difficulty\n"; return;
        }
        if (Block::computeTransactionsHash(block.transactions()) != block.header().getTransactions_hash()) {
            cout << "   [SKIPPED] neteisingas transactions_hash\n"; return;
        }

        for (const auto& tx : block.transactions()) {
          // Verify transaction ID (recompute) to detect tampering
          string expectedId = HashFunkcija(tx.getSender() + tx.getReceiver() + to_string(tx.getAmount()) + to_string(tx.getTimestamp()));
          if (expectedId != tx.getId()) {
             cout << "   TX " << tx.getId().substr(0,10) << "... "
                 << "INVALID ID - skipped\n";
             continue;
          }

          // Apply balances: withdraw returns false if sender missing or insufficient
          bool ok = users.withdraw(tx.getSender(), tx.getAmount());
          if (ok) users.deposit(tx.getReceiver(), tx.getAmount());
          cout << "   TX " << tx.getId().substr(0,10) << "... "
              << tx.getSender().substr(0,8) << " -> " << tx.getReceiver().substr(0,8)
              << " amt=" << tx.getAmount() << (ok ? " [APPLIED]" : " [SKIPPED: insufficient balance or unknown]") << "\n";
        }

        chain_.push_back(block);

        cout << "   Grandinės aukstis (be genesis): " << (chain_.size()-1) << "\n";
        cout << "   Bloko hash: " << block.block_hash() << "\n";
        cout << "   Prev hash : " << block.header().getPrev_hash().substr(0,16) << "...\n";
    }

private:
    vector<Block> chain_;
    unsigned difficulty_;
};

int main(int argc, char** argv) {
    unsigned difficulty = 3;   // kiek "0" hash pradžioje
    unsigned users      = 1000;
    unsigned txCount    = 10000;
    unsigned txPerBlock = 100;
    int      maxBlocks  = -1;

    for (int i = 1; i < argc; ++i) {
        string a = argv[i];
        auto eatU = [&](unsigned& v){ if (i+1 < argc) v = (unsigned)stoul(argv[++i]); };
        auto eatI = [&](int& v){ if (i+1 < argc) v = stoi(argv[++i]); };
        if (a == "--difficulty") eatU(difficulty);
        else if (a == "--users") eatU(users);
        else if (a == "--tx") eatU(txCount);
        else if (a == "--tpb") eatU(txPerBlock);
        else if (a == "--max-blocks") eatI(maxBlocks);
    }

    cout << "=== Supaprastinta bloku grandine v0.1 ===\n";
    cout << "difficulty=" << difficulty
         << " users=" << users
         << " tx=" << txCount
         << " txPerBlock=" << txPerBlock
         << " maxBlocks=" << maxBlocks << "\n\n";

    // Moduliai
    UserManager um;
    um.generateUsers(users);

    TxPool pool;
    mt19937_64 rng(random_device{}());
    generateTransactions(pool, um.keys(), txCount, rng);

    Blockchain bc(difficulty, txPerBlock);
    Miner miner(difficulty);

    // Mining ciklas
    int produced = 0;
    while (pool.size() > 0) {
        if (maxBlocks > 0 && produced >= maxBlocks) break;
        Block b = miner.makeCandidate(bc.tip().block_hash(), difficulty, txPerBlock, pool);
        if (b.transactions().empty()) break;
        miner.mine(b);
        bc.addBlock(b, um);
        ++produced;
        cout << "------------------------------------------------------------\n";
    }

    cout << " Baigta. Iskasta bloku: " << produced
         << " | grandines aukstis (be genesis): " << bc.height() << "\n";

    cout << "Viso gero!\n";
    return 0;
}














