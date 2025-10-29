#include <bits/stdc++.h>
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

    Transaction make(const string& sender, const string& receiver, long long amount, uint64_t timestamp) {
        Transaction t;
        t.sender_ = sender;
        t.receiver_ = receiver;
        t.amount_ = amount;
        t.timestamp_ = timestamp;
        string toHash = sender + "|" + receiver + "|" + to_string(amount) + "|" + to_string(timestamp);
        t.id_ = HashFunkcija(toHash);
        return t;
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






