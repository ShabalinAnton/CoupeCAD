#include "coupecad/core/id.h"

#include <uuid.h>

#include <random>

namespace coupecad::core {

namespace {

class RandomUuidGenerator : public UuidGenerator {
public:
    RandomUuidGenerator()
        : rng_(std::random_device{}()),
          gen_(&rng_) {}

    uuids::uuid next() override { return gen_(); }

private:
    std::mt19937 rng_;
    uuids::uuid_random_generator gen_;
};

class SeededUuidGenerator : public UuidGenerator {
public:
    explicit SeededUuidGenerator(std::uint64_t seed)
        : rng_(seed), gen_(&rng_) {}

    uuids::uuid next() override { return gen_(); }

private:
    std::mt19937 rng_;
    uuids::uuid_random_generator gen_;
};

}  // namespace

std::unique_ptr<UuidGenerator> make_random_uuid_generator() {
    return std::make_unique<RandomUuidGenerator>();
}

std::unique_ptr<UuidGenerator> make_seeded_uuid_generator(std::uint64_t seed) {
    return std::make_unique<SeededUuidGenerator>(seed);
}

}  // namespace coupecad::core
