#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>

using namespace std;

#ifndef HASH_DYDIS
#define HASH_DYDIS 32
#endif

// Trumpas failo aprašymas:
// Ši programa demonstruoja supaprastintą blokų grandinę.
// - `HashFunkcija` yramaišos funkcija 
// - `User`, `Transaction`, `UserManager` ir `TxPool` yra paprasti modeliai
// - `Block`/`BlockHeader` atvaizduoja blokus ir jų antraštes
// - `Miner` bando surasti nonce, kad blokas atitiktų difficulty (maišos pradžioje turi būti '0')

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


struct Logger {

    static void info(const std::string& s) { std::cout << "[INFO] " << s << "\n"; }
    static void warn(const std::string& s) { std::cout << "[WARN] " << s << "\n"; }
    static void dbg(const std::string& s)  { std::cout << "[DBG] "  << s << "\n"; }
    static void summary(const std::string& s) { std::cout << "=== " << s << " ===\n"; }
};

// PAGALBINES FUNKCIJOS REIKALINGOS 0 PATIKRINIMUI
// Patikrina, ar heksadinio hašo pradžia turi reikiamą kiekį '0' simbolių.
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

    // Paprasta klasė, sauganti naudotojo vardą, viešą raktą ir balansą.
    // Metodai deposit/withdraw keičia balansą; withdraw grąžina false, jei nėra pakankamai lėšų.
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

// ========= UTXO MODELIO IGYVENIMAS ==========
struct TxOutput {
    string receiventPubKey;
    long long amount{0};

    string serialize() const {
        return receiventPubKey + ":" + to_string(amount);
    }
};

struct TxInput {
    string prevTxId;       
    uint32_t outputIndex;   
    string senderPubKey;  
    string signature;  

    string sujunginmas() const {
        return prevTxId + ":" + to_string(outputIndex) + ":" + senderPubKey + ":" + signature;
    } 
};

// TRANSAKCIJOS KLASĖ
class Transaction {
public:

    
    Transaction() = default;

    Transaction(vector<TxInput> inputs, vector<TxOutput> outputs, uint64_t timestamp) 
    : inputs_(std::move(inputs)), outputs_(std::move(outputs)), timestamp_(timestamp) {
        computeId();
    }

    const string& getId() const { return id_; }
    uint64_t getTimestamp() const { return timestamp_; }
    const vector<TxInput>& getInputs() const { return inputs_; }
    vector<TxInput>& getInputs() { return inputs_; } // parasu saugojimui
    const vector<TxOutput>& getOutputs() const { return outputs_; }

    std::string serialiseCanonical() const {
        std::string s;
        s += to_string(timestamp_) + "|";
        s += to_string(inputs_.size()) + ";";
        for (const auto& inp : inputs_) {
            s += inp.prevTxId + "," + std::to_string(inp.outputIndex) + ";" + inp.senderPubKey + ";";
        }
        s += std::to_string(outputs_.size()) + "|";
        for (const auto& outp : outputs_) {
            s += outp.receiventPubKey + "," + std::to_string(outp.amount) + ";";
        }
        return s;
    }

    void computeId(){
        id_ = HashFunkcija(serialiseCanonical());
    }

    // Vartotojui draugiška patikra: perskaičiuoja kanoninę serializaciją
    // ir patikrina ar sutampa su saugomu id. Naudokite tai aptikti pakeistoms
    // arba neteisingoms transakcijoms prieš pritaikant jas prie UTXO rinkinio.
    bool verifyId() const {
    // Perskaičiuoja maišą iš kanoninės serializacijos ir palygina.
        return id_ == HashFunkcija(serialiseCanonical());
    }

private:
    string id_;
    uint64_t timestamp_{0};
    vector<TxInput> inputs_;
    vector<TxOutput> outputs_;

};

// Paprasta UTXO saugykla
class UTXOPool {
public:
    void add(const string& txid, uint32_t index, const TxOutput& out) {
        map_[key(txid, index)] = out;
    }

    bool exists(const string& txid, uint32_t index) const {
        return map_.find(key(txid, index)) != map_.end();
    }

    bool get(const string& txid, uint32_t index, TxOutput& out) const {
        auto it = map_.find(key(txid, index));
        if (it == map_.end()) return false;
        out = it->second; return true;
    }

    bool remove(const string& txid, uint32_t index) {
        return map_.erase(key(txid, index)) > 0;
    }

    // Grąžina visas raktų poras (txid:index), esančias pool'e
    vector<string> listKeys() const {
        vector<string> out; out.reserve(map_.size());
        for (auto& kv : map_) out.push_back(kv.first);
        return out;
    }

    // pagalbinė funkcija raktui parsinti
    static string key(const string& txid, uint32_t index) {
        return txid + ":" + to_string(index);
    }

    // išskaido raktą į txid ir index
    static bool parseKey(const string& k, string& txid, uint32_t& index) {
        auto p = k.find(':');
        if (p == string::npos) return false;
        txid = k.substr(0, p);
        index = (uint32_t)stoul(k.substr(p+1));
        return true;
    }

private:
    unordered_map<string, TxOutput> map_;
};

class UserManager {
public:
    // Pagrindinė deklaracija: saugoti vartotojus ir jų balansus
    void generateUsers(size_t nUsers) {
    // Sukuriame atsitiktinius pradinius balansus pagal reikalavimus
        uniform_int_distribution<long long> bal(100, 1000000);
        for (size_t i = 0; i < nUsers; ++i) {
            string name = randomName();
            string pk   = randomPublicKey();
            users_.emplace(pk, User{name, pk, bal(rng_)});
        }
        cout << "Sugeneruota vartotoju: " << users_.size() << "\n";
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
        string s; 
        int k = nSyl(rng_); 
        for (int i = 0; i < k; ++i) 
            s += syl[pick(rng_)];
        s[0] = (char)toupper(s[0]); return s;
    }
    string randomPublicKey() {
        static const char* hex = "0123456789abcdef";
        uniform_int_distribution<int> d(0, 15);
        string s = "PUB"; 
        for (int i = 0; i < 33; ++i) s += hex[d(rng_)];
        return s;
    }
};

class TxPool {
public:
    void push(Transaction tx) { pending_.push_back(std::move(tx)); }

    // Pagrindinė deklaracija: tvarkyti laukiančių transakcijų skaičių
    vector<Transaction> take(size_t maxCount) {
        size_t takeN = std::min(maxCount, pending_.size());
        vector<Transaction> out; out.reserve(takeN);
        if (takeN == 0) return out;

        for (size_t i = 0; i < takeN; ++i) {
            uniform_int_distribution<size_t> dist(0, pending_.size() - 1);
            size_t idx = dist(rng_);
            out.push_back(std::move(pending_[idx]));
            // pašaliname elementą (swap-pop)
            if (idx != pending_.size() - 1) pending_[idx] = std::move(pending_.back());
            pending_.pop_back();
        }
        return out;
    }

    size_t size() const { return pending_.size(); }
private:
    vector<Transaction> pending_;
    mt19937_64 rng_{random_device{}()};
};

// Funkcija, kuri sugeneruoja nTx atsitiktinių UTXO transakcijų ir įdeda jas į pool.
// - `allKeys` yra vartotojų vieši raktai (iš UserManager::keys())
// - Paimamas atsitiktinis UTXO (iš utxoPool), suformuojama vieno-input transakcija
// - Iš kiekvieno UTXO sukuriama tx: siunčiame dalį gavėjui, o grąža lieka siuntėjui
static void generateTransactions(TxPool& pool, const vector<string>& allKeys, const UTXOPool& utxoPool, size_t nTx, mt19937_64& rng) {
    if (allKeys.size() < 2) return;
    auto available = utxoPool.listKeys();
    if (available.empty()) return;

    uniform_int_distribution<size_t> pickAvail(0, available.size()-1);
    uniform_int_distribution<size_t> pickKey(0, allKeys.size()-1);

    size_t created = 0;
    while (created < nTx && !available.empty()) {
    // išsirenkame atsitiktinį prieinamą UTXO ir pašaliname jį iš vietinio sąrašo,
    // kad nebūtų naudojamas du kartus
        size_t idx = pickAvail(rng) % available.size();
        string key = available[idx];
    // pašaliname naudojant swap-pop metodą
        if (idx != available.size()-1) available[idx] = available.back();
        available.pop_back();

        string prevTxId; uint32_t outIdx;
        if (!UTXOPool::parseKey(key, prevTxId, outIdx)) continue;
        TxOutput prevOut;
        if (!utxoPool.get(prevTxId, outIdx, prevOut)) continue;

        const string& sender = prevOut.receiventPubKey;
    // parenkame gavėją, skirtingą nuo siuntėjo
        string receiver;
        do { receiver = allKeys[pickKey(rng)]; } while (receiver == sender && allKeys.size() > 1);

        uniform_int_distribution<long long> amt(1, prevOut.amount);
        long long sendAmt = amt(rng);

    // sudarome transakciją: vienas įėjimas, vienas arba du išėjimai (gavėjui ir grąža siuntėjui)
        TxInput in; in.prevTxId = prevTxId; in.outputIndex = outIdx; in.senderPubKey = sender; in.signature = "";
        vector<TxInput> inputs{in};
        vector<TxOutput> outputs;
        outputs.push_back(TxOutput{receiver, sendAmt});
        long long change = prevOut.amount - sendAmt;
        if (change > 0) outputs.push_back(TxOutput{sender, change});

        Transaction tx(inputs, outputs, nowSec());
        pool.push(std::move(tx));
        ++created;
    }

    cout << " Sugeneruota laukiama transakciju: " << pool.size() << "\n";
}

// BLOKO ANTRAŠTĖS KLASĖ
class BlockHeader {
public:
    BlockHeader() = default;

    // BlockHeader saugo informaciją, kuri apibūdina bloką bet ne pati transakcijas.
    // Pagrindiniai laukai:
    // - prev_hash: nuoroda į ankstesnio bloko hashą
    // - timestamp: kada blokas buvo sukurtas
    // - transactions_hash: Merkle root
    // - nonce: skaičius, kuris kinta, kai kasėjai bandydami surasti galiojantį hash
    // - difficulty: kiek '0' reikia hašo pradžioje

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

// Merkle medžio šaknies skaičiavimas iš hash sąrašo (string'ų)
static std::string create_merkle(std::vector<std::string> merkle) {
    // Jeigu sąrašas tuščias – galim grąžinti hash("") (kad elgsena liktų panaši
    // į buvusią computeTransactionsHash versiją)
    if (merkle.empty())
        return HashFunkcija("");

    // Jeigu vienas elementas – jis ir yra root
    if (merkle.size() == 1)
        return merkle[0];

    // Kol daugiau nei vienas hash'e – kartojam
    while (merkle.size() > 1) {
        // Jei nelyginis skaičius – dubliuojam paskutinį
        if (merkle.size() % 2 != 0) {
            merkle.push_back(merkle.back());
        }

        // Naujas lygis
        std::vector<std::string> new_merkle;
        new_merkle.reserve(merkle.size() / 2);

        // Einam po du
        for (auto it = merkle.begin(); it != merkle.end(); it += 2) {
            // sujungiame du hashus ir maišome
            std::string concat = *it + *(it + 1);
            std::string new_root = HashFunkcija(concat);
            new_merkle.push_back(std::move(new_root));
        }

        merkle = std::move(new_merkle);

        // (Pasirinktinai) debug:
        /*
        std::cout << "Current merkle hash list:\n";
        for (const auto& h : merkle)
            std::cout << "  " << h << "\n";
        std::cout << std::endl;
        */
    }

    // Likęs vienas elementas – Merkle root
    return merkle[0];
}


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

    // computeTransactionsHash aadapta pagal merkle tree is papildomos
    static std::string computeTransactionsHash(const std::vector<Transaction>& txs) {
    // surenkame transakcijų ID į sąrašą merkle medžiui
    std::vector<std::string> merkle;
    merkle.reserve(txs.size());
    for (const auto& t : txs) {
        merkle.push_back(t.getId());
    }

    // pasinaudojame integruota create_merkle() funkcija
    return create_merkle(std::move(merkle));
}



private:
    BlockHeader header_;
    vector<Transaction> transactions_;
    string block_hash_;
};

// MINER KLASĖ
class Miner {
public:
    explicit Miner(unsigned difficulty, string minerPublicKey, long long reward = 50)
    : difficulty_(difficulty), minerPub_(std::move(minerPublicKey)), reward_(reward) {}

    // Miner klasė atvaizduoja vieno kasėjo elgesį:
    // - makeCandidate: sukuria bloko kandidatą (prideda transakcijas, nustato prev_hash ir difficulty)
    // - tryMine: bando rasti nonce su laiko arba bandymų apribojimu
    // - mine: bloko kasimas be apribojimų

    Block makeCandidate(const string& prevHash, unsigned difficulty, size_t txPerBlock, TxPool& pool) {
        Block b;
        b.header().set_prev_hash(prevHash);
        b.header().set_timestamp(nowSec());
        b.header().set_version("v2025");
        b.header().set_difficulty(difficulty);

        auto picked = pool.take(txPerBlock);
        b.transactions() = std::move(picked);

    // Pridedame coinbase (kasėjo atlygis) transakciją, kad kasėjas gautų atlygį, jei blokas priimtas
    vector<TxInput> cbIn; // tušti įėjimai (coinbase)
    vector<TxOutput> cbOuts; cbOuts.push_back(TxOutput{minerPub_, reward_});
    Transaction coinbase(std::move(cbIn), std::move(cbOuts), nowSec());
    b.transactions().insert(b.transactions().begin(), std::move(coinbase));

        b.header().set_transactions_hash(Block::computeTransactionsHash(b.transactions()));
        return b;
    }

    
    bool tryMine(Block& block, uint64_t timeLimitMs, uint64_t maxAttempts) {
        auto start = chrono::high_resolution_clock::now();
        uint64_t attempts = 0;
        uint64_t nonce = block.header().getNonce();

        while (true) {
            if (maxAttempts != 0 && attempts >= maxAttempts) return false;
            if (timeLimitMs != 0) {
                auto elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - start).count();
                if ((uint64_t)elapsed >= timeLimitMs) return false;
            }

            block.header().set_nonce(nonce++);
            string h = HashFunkcija(block.header().to_string());
            ++attempts;
            if (starts_with_zeros(h, block.header().getDifficulty())) {
                block.set_block_hash(h);
                return true;
            }
            if (attempts % 100000 == 0) {
                cout << "   ... bandyta nonce " << nonce << "\r" << flush;
            }
        }
    }

    // Paprastas paralelinis kasimas: kelios gijos ieško skirtingų nonce sekų
    // Kiekviena gija pradeda nuo skirtingo nonce ir žengia po `threadCount`, kad
    // nekristų darbai vienas ant kito. Gijos sustoja, kai kurios randa galiojantį nonce,
    // pasibaigia timeLimitMs arba pasiekiamas maxAttempts limitas (viso visų gijų).
    bool tryMineParallel(Block& block, uint64_t timeLimitMs, uint64_t maxAttempts, unsigned threadCount) {
        if (threadCount == 0) threadCount = std::thread::hardware_concurrency();
    if (threadCount == 0) threadCount = 2; // atsarginė reikšmė
    // apribojame gijų skaičių iki nedidelio, kad elgesys būtų paprastas ir aiškus
        if (threadCount > 8) threadCount = 8;

        std::atomic<bool> found{false};
        std::atomic<uint64_t> totalAttempts{0};
        std::atomic<uint64_t> foundNonce{0};
        std::string foundHash;
        std::mutex foundMutex;

        auto start = chrono::high_resolution_clock::now();
    BlockHeader baseHeader = block.header(); // kopija, kad gijos nekeistų bendros antraštės

        std::vector<std::thread> threads;
        threads.reserve(threadCount);

        for (unsigned tid = 0; tid < threadCount; ++tid) {
            threads.emplace_back([&, tid]() {
                uint64_t localNonce = baseHeader.getNonce() + tid;
                uint64_t localAttempts = 0;
                BlockHeader hdr = baseHeader; // vienos gijos kopija antraštei

                while (!found.load(std::memory_order_relaxed)) {
                    if (maxAttempts != 0 && totalAttempts.load(std::memory_order_relaxed) >= maxAttempts) break;
                    if (timeLimitMs != 0) {
                        auto elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - start).count();
                        if ((uint64_t)elapsed >= timeLimitMs) break;
                    }

                    hdr.set_nonce(localNonce);
                    std::string h = HashFunkcija(hdr.to_string());

                    ++localAttempts;
                    ++totalAttempts;

                    if (starts_with_zeros(h, hdr.getDifficulty())) {
                        std::lock_guard<std::mutex> lk(foundMutex);
                        if (!found.load()) {
                            found.store(true);
                            foundNonce.store(localNonce);
                            foundHash = h;
                        }
                        break;
                    }

                    localNonce += threadCount;
                    if (localAttempts % 100000 == 0) {
                        cout << "    [th " << tid << "] tried " << localAttempts << " nonces\r" << flush;
                    }
                }
            });
        }

        for (auto &t : threads) if (t.joinable()) t.join();

        if (found.load()) {
            block.header().set_nonce(foundNonce.load());
            block.set_block_hash(foundHash);
            return true;
        }
        return false;
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
    string minerPub_;
    long long reward_{0};
};

// BLOCKCHAIN KLASĖ
class Blockchain {
public:
    explicit Blockchain(unsigned difficulty, unsigned = 100)
    : difficulty_(difficulty) {
    // Genesis (pradinis) blokas
        Block genesis;
        genesis.header().set_prev_hash(string(HASH_DYDIS * 2, '0'));
        genesis.header().set_timestamp(nowSec());
        genesis.header().set_version("v2025");
        genesis.header().set_difficulty(difficulty_);
        genesis.header().set_transactions_hash(Block::computeTransactionsHash(genesis.transactions()));
        genesis.set_block_hash(HashFunkcija(genesis.header().to_string()));
        chain_.push_back(std::move(genesis));
    }

    const Block& tip() const { return chain_.back(); }
    size_t height() const { return chain_.size() - 1; } // be genesis

    // UTXO pagrindu pridedame bloką: validuojame, ar įėjimai egzistuoja UTXO pool'e,
    // suskaičiuojame sumas ir pritaikome pakeitimus prie pool'o
    void addBlock(const Block& block, UTXOPool& utxos) {
        Logger::info("Pridedame bloka #" + to_string(chain_.size()) + "  tx=" + to_string(block.transactions().size()));

    // bazinės validacijos
        if (block.header().getPrev_hash() != tip().block_hash()) {
            Logger::warn("[SKIPPED] blogas prev_hash");
            return;
        }
        if (!starts_with_zeros(block.block_hash(), block.header().getDifficulty())) {
            Logger::warn("[SKIPPED] netenkina difficulty");
            return;
        }
        if (Block::computeTransactionsHash(block.transactions()) != block.header().getTransactions_hash()) {
            Logger::warn("[SKIPPED] neteisingas transactions_hash");
            return;
        }

        size_t applied = 0, skipped = 0, coinbaseCount = 0;

    // sekame panaudotus išėjimus šiame bloke, kad išvengtume dvigubo išleidimo bloke
        unordered_set<string> spentInBlock;

        for (const auto& tx : block.transactions()) {
            // Patikriname, kad transakcijos id atitinka kanoninę serializaciją.
            // Tai užkerta kelią pritaikyti pakeistas ar neteisingai sukonstruotas transakcijas.
            if (!tx.verifyId()) {
                ++skipped;
                cout << "   TX " << tx.getId().substr(0,10) << "... INVALID ID - skipped\n";
                continue;
            }
            // coinbase (neturi įėjimų)
            if (tx.getInputs().empty()) {
                // pridedame išėjimus kaip naujus UTXO
                const auto& outs = tx.getOutputs();
                for (size_t i = 0; i < outs.size(); ++i) utxos.add(tx.getId(), (uint32_t)i, outs[i]);
                ++coinbaseCount; ++applied;
                cout << "   TX " << tx.getId().substr(0,10) << "... COINBASE applied, outs=" << outs.size() << "\n";
                continue;
            }

            // tikriname, ar įėjimai egzistuoja ir priklauso nurodytam siuntėjui
            long long sumIn = 0;
            bool ok = true;
            for (const auto& inp : tx.getInputs()) {
                string key = UTXOPool::key(inp.prevTxId, inp.outputIndex);
                if (spentInBlock.find(key) != spentInBlock.end()) { ok = false; break; }
                TxOutput prevOut;
                if (!utxos.get(inp.prevTxId, inp.outputIndex, prevOut)) { ok = false; break; }
                if (inp.senderPubKey != prevOut.receiventPubKey) { ok = false; break; }
                sumIn += prevOut.amount;
            }
            if (!ok) { ++skipped; cout << "   TX " << tx.getId().substr(0,10) << "... INVALID INPUTS - skipped\n"; continue; }

            long long sumOut = 0;
            for (const auto& o : tx.getOutputs()) sumOut += o.amount;
            if (sumOut > sumIn) { ++skipped; cout << "   TX " << tx.getId().substr(0,10) << "... OUTPUTS > INPUTS - skipped\n"; continue; }

            // pritaikymas: pašaliname įėjimus ir pažymime kaip panaudotus; pridedame išėjimus kaip naujus UTXO
            for (const auto& inp : tx.getInputs()) {
                utxos.remove(inp.prevTxId, inp.outputIndex);
                spentInBlock.insert(UTXOPool::key(inp.prevTxId, inp.outputIndex));
            }
            for (size_t i = 0; i < tx.getOutputs().size(); ++i) utxos.add(tx.getId(), (uint32_t)i, tx.getOutputs()[i]);

            ++applied;
            cout << "   TX " << tx.getId().substr(0,10) << "... applied, in=" << sumIn << " out=" << sumOut << "\n";
        }

        chain_.push_back(block);

    // santrauka
        Logger::summary("Block summary");
        cout << "   Applied tx: " << applied << "\n";
        cout << "   Skipped tx: " << skipped << "\n";
        cout << "   Coinbase tx: " << coinbaseCount << "\n";
        cout << "   Grandines aukstis (be genesis): " << (chain_.size()-1) << "\n";
        cout << "   Bloko hash: " << block.block_hash() << "\n";
        cout << "   Prev hash : " << block.header().getPrev_hash().substr(0,16) << "...\n";
    }

private:
    vector<Block> chain_;
    unsigned difficulty_;
};

int main(int argc, char** argv) {

    unsigned difficulty = 9;   // kiek "0" hash pradžioje
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

    // PAGRINDINIS: nustatymai ir moduliai
    cout << "=== Supaprastinta bloku grandine v0.1 ===\n";
    cout << "difficulty=" << difficulty
         << " users=" << users
         << " tx=" << txCount
         << " txPerBlock=" << txPerBlock
         << " maxBlocks=" << maxBlocks << "\n\n";

    // Moduliai:
    // - UserManager: generuoja vartotojus ir jų balansus
    // - TxPool: laukiančių transakcijų saugykla
    // - Blockchain: pati grandinė su pradiniu (genesis) bloku
    // - Miner: objektas, kuris bando išgauti (kasti) blokus
    UserManager um;
    um.generateUsers(users); // 1000 

    TxPool pool;
    mt19937_64 rng(random_device{}());
    // Sukuriame pradinį UTXO rinkinį iš sugeneruotų vartotojų (finansuojame kiekvieną vartotoją jų pradiniu balansu)
    UTXOPool utxoPool;
    for (const auto& kv : um.all()) {
        const string& pk = kv.first;
        long long bal = kv.second.getBalance();
    // sukurti paprastą funding UTXO kiekvienam vartotojui
        string fundId = HashFunkcija(string("FUND:") + pk + ":" + to_string(nowSec()));
        TxOutput o{pk, bal};
        utxoPool.add(fundId, 0, o);
    }

    auto keys = um.keys();
    generateTransactions(pool, keys, utxoPool, txCount, rng);

    Blockchain bc(difficulty, txPerBlock);
    // parenkame vieną sugeneruotą vartotoją kaip kasėją (kad atlygis būtų priskirtas realiam sąskaitai)
    string minerKey = keys.empty() ? string("MINER_PUB") : keys[0];
    Miner miner(difficulty, minerKey, /*reward=*/50);

    // Mining ciklas
    int produced = 0;
    const size_t nCandidates = 5; // kiek kandidatų sugeneruoti kiekvienam raundui

    while (pool.size() > 0) {
        if (maxBlocks > 0 && produced >= maxBlocks) break;

        // Sugeneruojame iki nCandidates kandidatų
        vector<Block> candidates;
        for (size_t i = 0; i < nCandidates; ++i) {
            if (pool.size() == 0) break;
            Block c = miner.makeCandidate(bc.tip().block_hash(), difficulty, txPerBlock, pool);
            if (c.transactions().empty()) break;
            candidates.push_back(std::move(c));
        }
        if (candidates.empty()) break;

        // Pradiniai limitai
    uint64_t timeLimitMs = 5000; // 5000 ms = 5 sekundės
        uint64_t maxAttempts = 5000; // pradinis bandymų limitas
        bool mined = false;
        size_t minedIndex = SIZE_MAX;

        // Kartoti tol, kol kas nors iš kandidatų iškasamas; jei ne - didiname limitus
        int round = 0;
        const int maxRounds = 6; // saugiklis
        while (!mined && round < maxRounds) {
            cout << "Pradedame kasimo raunda " << round+1 << ": timeLimit=" << timeLimitMs << " ms, attempts=" << maxAttempts << "\n";
            // Faktiniai bandymai: bandomas kiekvienas kandidatas (paraleliai per kandidatą)
            unsigned threadsToUse = std::thread::hardware_concurrency();
            if (threadsToUse == 0) threadsToUse = 2;
            // keep small for beginner friendliness
            if (threadsToUse > 4) threadsToUse = 4;

            for (size_t i = 0; i < candidates.size(); ++i) {
                cout << "  Bandome kandidata #" << i << " (tx=" << candidates[i].transactions().size() << ")\n";
                if (miner.tryMineParallel(candidates[i], timeLimitMs, maxAttempts, threadsToUse)) {
                    mined = true; minedIndex = i; break;
                }
                // neperkopsime - tęsiame su kitais kandidatais
            }

            if (!mined) {
                // jei niekas neiškasta, padidinti limitus ir kartoti
                cout << "  Niekas neiskasta siame raunde, didiname limitus ir kartojame\n";
                // padidiname ribas
                timeLimitMs = timeLimitMs * 2;
                if (maxAttempts > 0) maxAttempts = maxAttempts * 2;
            }
            ++round;
        }

        if (mined) {
            // Atkuriame neigytų kandidatų transakcijas atgal į pool
            for (size_t i = 0; i < candidates.size(); ++i) {
                if (i == minedIndex) continue;
                for (auto& tx : candidates[i].transactions()) pool.push(std::move(tx));
            }

            // Pridedame iškastą bloką
            Block minedBlock = std::move(candidates[minedIndex]);
            bc.addBlock(minedBlock, utxoPool);
            ++produced;
            cout << "------------------------------------------------------------\n";
        } else {
            // Jei nepavyko po maxRounds, grąžiname visų kandidatų tx į pool ir tęsiame (arba išeiname)
            cout << "Nesekmingi bandymai po " << maxRounds << " raundu. Graziname transakcijas i pool.\n";
            for (auto& c : candidates) for (auto& tx : c.transactions()) pool.push(std::move(tx));
            // Galim eiti toliau arba išeiti - čia išeiname (jei norite kitaip, galite pakeisti)
            break;
        }
    }

    cout << " Baigta. Iskasta bloku: " << produced
         << " | grandines aukstis (be genesis): " << bc.height() << "\n";

    cout << "Viso gero!\n";
    return 0;
}














