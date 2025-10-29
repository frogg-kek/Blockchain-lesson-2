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

    const string& name() const { return name_; }
    const string& public_key() const { return publicKey_; }
    long long balance() const { return balance_; }

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

    Transaction(const string& sender, const string& receiver, long long amount, uint64_t timestamp)
        : sender_(sender), receiver_(receiver), amount_(amount), timestamp_(timestamp) {
        string toHash = sender_ + "|" + receiver_ + "|" + to_string(amount_) + "|" + to_string(timestamp_);
        id_ = HashFunkcija(toHash);
    }

    const string& id() const { return id_; }
    const string& sender() const { return sender_; }
    const string& receiver() const { return receiver_; }
    long long amount() const { return amount_; }
    uint64_t timestamp() const { return timestamp_; }

private:
    string id_;
    string sender_;
    string receiver_;
    long long amount_{0};
    uint64_t timestamp_{0};
};

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

    const string& prev_hash() const { return prev_hash_; }
    uint64_t timestamp() const { return timestamp_; }
    const string& version() const { return version_; }
    const string& transactions_hash() const { return transactions_hash_; }
    uint64_t nonce() const { return nonce_; }
    unsigned difficulty() const { return difficulty_; }

    string to_string() const {
        return prev_hash_ + "|" + std::to_string(timestamp_) + "|" + version_ + "|" +
               transactions_hash_ + "|" + std::to_string(nonce_) + "|" + std::to_string(difficulty_);
    }

private:
    string prev_hash_;
    uint64_t timestamp_{0};
    string version_{"v0.1"};
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
        string concat; concat.reserve(txs.size() * 8);
        for (auto& t : txs) concat += t.id();
        return HashFunkcija(concat);
    }

private:
    BlockHeader header_;
    vector<Transaction> transactions_;
    string block_hash_;
};

// BLOCKCHAIN KLASĖ
class Blockchain {
public:
    Blockchain(unsigned difficulty = 3, unsigned txPerBlock = 100)
    : difficulty_(difficulty), txPerBlock_(txPerBlock), rng_(random_device{}()) {
        // Genesis
        Block genesis;
        genesis.header().set_prev_hash(string(HASH_DYDIS * 2, '0')); // 64 nuliai, kai HASH_DYDIS=32
        genesis.header().set_timestamp(nowSec());
        genesis.header().set_version("v0.1");
        genesis.header().set_difficulty(difficulty_);
        genesis.header().set_transactions_hash(Block::computeTransactionsHash(genesis.transactions()));
        genesis.set_block_hash(HashFunkcija(genesis.header().to_string()));
        chain_.push_back(std::move(genesis));
    }

    void generateUsers(size_t nUsers) {
        uniform_int_distribution<long long> bal(100, 1'000'000);
        for (size_t i = 0; i < nUsers; ++i) {
            string name = randomName();
            string pk   = randomPublicKey();
            users_.emplace(pk, User{name, pk, bal(rng_)});
        }
        cout << "👥 Sugeneruota vartotojų: " << users_.size() << "\n";
    }

        void generateTransactions(size_t nTx) {
        if (users_.size() < 2) return;
        uniform_int_distribution<long long> amt(1, 5000);

        vector<string> keys; keys.reserve(users_.size());
        for (auto& kv : users_) keys.push_back(kv.first);
        uniform_int_distribution<size_t> pick(0, keys.size()-1);

        for (size_t i = 0; i < nTx; ++i) {
            const string& sender = keys[pick(rng_)];
            string receiver;
            do { receiver = keys[pick(rng_)]; } while (receiver == sender);

            long long amount = amt(rng_);
            pending_.emplace_back(sender, receiver, amount, nowSec());
        }
        cout << "🧾 Sugeneruota laukiama transakcijų: " << pending_.size() << "\n";
    }

        Block formCandidateBlock() {
        Block b;
        b.header().set_prev_hash(chain_.back().block_hash());
        b.header().set_timestamp(nowSec());
        b.header().set_version("v0.1");
        b.header().set_difficulty(difficulty_);

        size_t take = std::min<size_t>(txPerBlock_, pending_.size());
        std::sample(pending_.begin(), pending_.end(), back_inserter(b.transactions()), take, rng_);

        b.header().set_transactions_hash(Block::computeTransactionsHash(b.transactions()));
        return b;
    }

        void mine(Block& block) {
        cout << "⛏️  Kasam bloką: " << block.transactions().size()
             << " tx... tikslas: " << block.header().difficulty() << " nuliai pradžioje\n";

        auto start = chrono::high_resolution_clock::now();
        uint64_t nonce = 0;

        while (true) {
            block.header().set_nonce(nonce++);
            string h = HashFunkcija(block.header().to_string()); // TAVO hash
            if (starts_with_zeros(h, block.header().difficulty())) {
                block.set_block_hash(h);
                break;
            }
            if (nonce % 100000 == 0) {
                cout << "   ... bandyta nonce " << nonce << "\r" << flush;
            }
        }
        auto ms = chrono::duration_cast<chrono::milliseconds>(
                      chrono::high_resolution_clock::now() - start).count();
        cout << "\n✅ Iškasta! nonce=" << block.header().nonce()
             << " hash=" << block.block_hash() << " (" << ms << " ms)\n";
    }

        void addBlock(const Block& block) {
        cout << "🔗 Pridedame bloką #" << chain_.size()
             << "  tx=" << block.transactions().size() << "\n";

        for (const auto& tx : block.transactions()) {
            auto sIt = users_.find(tx.sender());
            auto rIt = users_.find(tx.receiver());
            bool ok = false;
            if (sIt != users_.end() && rIt != users_.end()) {
                if (sIt->second.withdraw(tx.amount())) {
                    rIt->second.deposit(tx.amount());
                    ok = true;
                }
            }
            cout << "   TX " << tx.id().substr(0,10) << "... "
                 << tx.sender().substr(0,8) << " -> " << tx.receiver().substr(0,8)
                 << " amt=" << tx.amount() << (ok ? " [APPLIED]" : " [SKIPPED]") << "\n";
        }

        vector<string> ids; ids.reserve(block.transactions().size());
        for (auto& t : block.transactions()) ids.push_back(t.id());
        pending_.erase(remove_if(pending_.begin(), pending_.end(),
            [&](const Transaction& t){ return find(ids.begin(), ids.end(), t.id()) != ids.end(); }),
            pending_.end());

        chain_.push_back(block);

        cout << "   Grandinės aukštis (be genesis): " << (chain_.size()-1) << "\n";
        cout << "   Liko laukiama transakcijų: " << pending_.size() << "\n";
        cout << "   Bloko hash: " << block.block_hash() << "\n";
        cout << "   Prev hash : " << block.header().prev_hash().substr(0,16) << "...\n";
    }
        void run(int maxBlocks = -1) {
        int produced = 0;
        while (!pending_.empty()) {
            if (maxBlocks > 0 && produced >= maxBlocks) break;
            Block b = formCandidateBlock();
            if (b.transactions().empty()) break;
            mine(b);
            addBlock(b);
            ++produced;
            cout << "------------------------------------------------------------\n";
        }
        cout << "🏁 Baigta. Iškasta blokų: " << produced
             << " | grandinės aukštis (be genesis): " << (chain_.size()-1) << "\n";
    }

private:
    
    vector<Block> chain_;
    vector<Transaction> pending_;
    unordered_map<string, User> users_;

    // nustatymai
    unsigned difficulty_;
    unsigned txPerBlock_;
    mt19937_64 rng_;

    
    string randomName() {
        static const char* syl[] = {"va","de","ra","li","no","ka","mi","to","sa","re","na","zo"};
        uniform_int_distribution<int> nSyl(2,3);
        uniform_int_distribution<int> pick(0, (int)(sizeof(syl)/sizeof(syl[0]))-1);
        string s;
        int k = nSyl(rng_);
        for (int i = 0; i < k; ++i) s += syl[pick(rng_)];
        s[0] = (char)toupper(s[0]);
        return s;
    }
    string randomPublicKey() {
        static const char* hex = "0123456789abcdef";
        uniform_int_distribution<int> d(0, 15);
        string s = "PUB";
        for (int i = 0; i < 33; ++i) s += hex[d(rng_)];
        return s;
    }
};
int main(int argc, char** argv) {
    unsigned difficulty = 3;   // kiek "0" hash pradžioje
    unsigned users      = 1000;
    unsigned txCount    = 10000;
    unsigned txPerBlock = 100;
    int      maxBlocks  = -1;  

    // paprasti flag'ai
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

    cout << "=== Supaprastinta blokų grandinė v0.1 (OOP + tavo HashFunkcija) ===\n";
    cout << "difficulty=" << difficulty
         << " users=" << users
         << " tx=" << txCount
         << " txPerBlock=" << txPerBlock
         << " maxBlocks=" << maxBlocks << "\n\n";

    Blockchain bc(difficulty, txPerBlock);
    bc.generateUsers(users);
    bc.generateTransactions(txCount);
    bc.run(maxBlocks);

    cout << "Viso gero!\n";
    return 0;
}













