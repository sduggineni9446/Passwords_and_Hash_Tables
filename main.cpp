#include <cstdint>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using std::size_t;

uint64_t demoHash(const std::string& s) {
    uint64_t h = 1469598103934665603ULL;

    for (unsigned char c : s) {
        h = (h ^ c) * 1099511628211ULL;
    }

    return h;
}

enum class CollisionStrategy {
    LinearProbing,
    QuadraticProbing,
    DoubleHashing,
    Chaining
};

std::string strategyName(CollisionStrategy strategy) {
    switch (strategy) {
        case CollisionStrategy::LinearProbing:
            return "linear probing";
        case CollisionStrategy::QuadraticProbing:
            return "quadratic probing";
        case CollisionStrategy::DoubleHashing:
            return "double hashing";
        case CollisionStrategy::Chaining:
            return "separate chaining";
    }

    return "unknown";
}

bool isPrime(size_t value) {
    if (value < 2) {
        return false;
    }

    for (size_t divisor = 2; divisor * divisor <= value; ++divisor) {
        if (value % divisor == 0) {
            return false;
        }
    }

    return true;
}

size_t nextPrime(size_t value) {
    while (!isPrime(value)) {
        ++value;
    }

    return value;
}

struct UserRecord {
    std::string username;
    std::string salt;
    uint64_t passwordHash{};
};

class HashTable {
private:
    enum class SlotState {
        Empty,
        Occupied,
        Deleted
    };

    struct Slot {
        SlotState state = SlotState::Empty;
        UserRecord record;
    };

    CollisionStrategy strategy;
    std::vector<Slot> slots;
    std::vector<std::vector<UserRecord>> chains;
    size_t itemCount = 0;

    size_t tableSize() const {
        if (strategy == CollisionStrategy::Chaining) {
            return chains.size();
        }

        return slots.size();
    }

    size_t indexFor(const std::string& key, size_t capacity) const {
        return static_cast<size_t>(demoHash(key) % capacity);
    }

    size_t stepFor(const std::string& key, size_t capacity) const {
        return 1 + static_cast<size_t>(
            demoHash("step:" + key) % (capacity - 1)
        );
    }

    size_t probeIndex(
        const std::string& key,
        size_t attempt,
        size_t capacity
    ) const {
        size_t firstIndex = indexFor(key, capacity);

        if (strategy == CollisionStrategy::LinearProbing) {
            return (firstIndex + attempt) % capacity;
        }

        if (strategy == CollisionStrategy::QuadraticProbing) {
            return (firstIndex + attempt * attempt) % capacity;
        }

        return (firstIndex + attempt * stepFor(key, capacity)) % capacity;
    }

    double loadFactorAfterInsert() const {
        return static_cast<double>(itemCount + 1) /
               static_cast<double>(tableSize());
    }

    void insertWithoutResize(const UserRecord& record) {
        if (strategy == CollisionStrategy::Chaining) {
            size_t index = indexFor(record.username, chains.size());
            chains[index].push_back(record);
            return;
        }

        for (size_t attempt = 0; attempt < slots.size(); ++attempt) {
            size_t index = probeIndex(
                record.username,
                attempt,
                slots.size()
            );

            if (slots[index].state != SlotState::Occupied) {
                slots[index].record = record;
                slots[index].state = SlotState::Occupied;
                return;
            }
        }
    }

    void rehash(size_t newCapacity) {
        if (strategy == CollisionStrategy::Chaining) {
            std::vector<std::vector<UserRecord>> oldChains =
                std::move(chains);

            chains.clear();
            chains.resize(newCapacity);

            for (const auto& bucket : oldChains) {
                for (const auto& record : bucket) {
                    insertWithoutResize(record);
                }
            }
        } else {
            std::vector<Slot> oldSlots = std::move(slots);

            slots.clear();
            slots.resize(newCapacity);

            for (const auto& slot : oldSlots) {
                if (slot.state == SlotState::Occupied) {
                    insertWithoutResize(slot.record);
                }
            }
        }
    }

    void prepareForInsert() {
        if (loadFactorAfterInsert() > 0.70) {
            rehash(nextPrime(tableSize() * 2));
        }
    }

public:
    explicit HashTable(
        CollisionStrategy chosenStrategy,
        size_t initialCapacity = 11
    )
        : strategy(chosenStrategy) {
        if (strategy == CollisionStrategy::Chaining) {
            chains.resize(initialCapacity);
        } else {
            slots.resize(initialCapacity);
        }
    }

    bool insert(const UserRecord& record) {
        if (find(record.username).has_value()) {
            return false;
        }

        prepareForInsert();
        insertWithoutResize(record);
        ++itemCount;

        return true;
    }

    std::optional<UserRecord> find(
        const std::string& username
    ) const {
        if (strategy == CollisionStrategy::Chaining) {
            size_t index = indexFor(username, chains.size());
            const auto& bucket = chains[index];

            for (const auto& record : bucket) {
                if (record.username == username) {
                    return record;
                }
            }

            return std::nullopt;
        }

        for (size_t attempt = 0; attempt < slots.size(); ++attempt) {
            size_t index = probeIndex(
                username,
                attempt,
                slots.size()
            );

            if (slots[index].state == SlotState::Empty) {
                return std::nullopt;
            }

            if (
                slots[index].state == SlotState::Occupied &&
                slots[index].record.username == username
            ) {
                return slots[index].record;
            }
        }

        return std::nullopt;
    }

    bool remove(const std::string& username) {
        if (strategy == CollisionStrategy::Chaining) {
            size_t index = indexFor(username, chains.size());
            auto& bucket = chains[index];

            for (auto it = bucket.begin(); it != bucket.end(); ++it) {
                if (it->username == username) {
                    bucket.erase(it);
                    --itemCount;
                    return true;
                }
            }

            return false;
        }

        for (size_t attempt = 0; attempt < slots.size(); ++attempt) {
            size_t index = probeIndex(
                username,
                attempt,
                slots.size()
            );

            if (slots[index].state == SlotState::Empty) {
                return false;
            }

            if (
                slots[index].state == SlotState::Occupied &&
                slots[index].record.username == username
            ) {
                slots[index].state = SlotState::Deleted;
                --itemCount;
                return true;
            }
        }

        return false;
    }

    double loadFactor() const {
        return static_cast<double>(itemCount) /
               static_cast<double>(tableSize());
    }

    size_t size() const {
        return itemCount;
    }
};

class AuthenticationDemo {
private:
    static constexpr const char* PEPPER = "classroom-pepper";

    HashTable table;
    std::mt19937_64 generator{20261018};

    std::string makeSalt() {
        std::ostringstream output;
        output << std::hex << generator();
        return output.str();
    }

    UserRecord makeRecord(
        const std::string& username,
        const std::string& password
    ) {
        std::string salt = makeSalt();

        uint64_t passwordHash =
            demoHash(salt + password + PEPPER);

        return {
            username,
            salt,
            passwordHash
        };
    }

public:
    explicit AuthenticationDemo(
        CollisionStrategy strategy
    )
        : table(strategy, 11) {}

    void addUser(
        const std::string& username,
        const std::string& password
    ) {
        table.insert(makeRecord(username, password));
    }

    bool login(
        const std::string& username,
        const std::string& password
    ) const {
        auto record = table.find(username);

        if (!record.has_value()) {
            return false;
        }

        uint64_t submittedHash =
            demoHash(record->salt + password + PEPPER);

        return submittedHash == record->passwordHash;
    }

    void removeUser(const std::string& username) {
        table.remove(username);
    }

    double loadFactor() const {
        return table.loadFactor();
    }

    size_t userCount() const {
        return table.size();
    }
};

void runDemo(CollisionStrategy strategy) {
    std::cout << "\n=== "
              << strategyName(strategy)
              << " ===\n";

    AuthenticationDemo authentication(strategy);

    authentication.addUser("alice", "Maple123!");
    authentication.addUser("brandon", "River456!");
    authentication.addUser("carla", "Sunset789!");
    authentication.addUser("diego", "Cloud321!");
    authentication.addUser("elena", "Forest654!");

    std::cout << "Successful login: "
              << (
                  authentication.login(
                      "alice",
                      "Maple123!"
                  )
                      ? "PASS"
                      : "FAIL"
              )
              << '\n';

    std::cout << "Failed password: "
              << (
                  !authentication.login(
                      "alice",
                      "wrong-password"
                  )
                      ? "PASS"
                      : "FAIL"
              )
              << '\n';

    std::cout << "Unknown user: "
              << (
                  !authentication.login(
                      "nobody",
                      "anything"
                  )
                      ? "PASS"
                      : "FAIL"
              )
              << '\n';

    authentication.removeUser("carla");

    std::cout << "Removal: "
              << (
                  !authentication.login(
                      "carla",
                      "Sunset789!"
                  )
                      ? "PASS"
                      : "FAIL"
              )
              << '\n';

    for (int i = 0; i < 10; ++i) {
        authentication.addUser(
            "extra" + std::to_string(i),
            "Password" + std::to_string(i)
        );
    }

    std::cout << "Rehash preserved existing login: "
              << (
                  authentication.login(
                      "alice",
                      "Maple123!"
                  )
                      ? "PASS"
                      : "FAIL"
              )
              << '\n';

    std::cout << "Users stored: "
              << authentication.userCount()
              << "; load factor: "
              << std::fixed
              << std::setprecision(2)
              << authentication.loadFactor()
              << '\n';
}

int main() {
    runDemo(CollisionStrategy::LinearProbing);
    runDemo(CollisionStrategy::QuadraticProbing);
    runDemo(CollisionStrategy::DoubleHashing);
    runDemo(CollisionStrategy::Chaining);

    std::cout << "\nRecommended strategy: separate chaining, "
                 "because deletion is straightforward and collisions "
                 "do not create long probe clusters.\n";

    std::cout << "Note: demoHash is for coursework only, "
                 "not production password security.\n";

    return 0;
}