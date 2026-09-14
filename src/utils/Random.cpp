#include "Random.h"

#include <chrono>
#include <sstream>
#include <iomanip>

Random::Random() {
    // 用真实随机设备 + 时间戳播种
    std::random_device rd;
    Seed(rd() ^ (unsigned int)std::chrono::steady_clock::now().time_since_epoch().count());
}

Random::Random(unsigned int seed) { Seed(seed); }

void Random::Seed(unsigned int seed) { mEngine.seed(seed); }

int Random::GetInt(int min, int max) {
    if (min > max) std::swap(min, max);
    std::uniform_int_distribution<int> dist(min, max);
    return dist(mEngine);
}

double Random::GetDouble() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(mEngine);
}

bool Random::Chance(double p) { return GetDouble() < p; }

std::string Random::Token(int bytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < bytes; ++i) oss << std::setw(2) << (unsigned int)(GetInt(0, 255));
    return oss.str();
}
