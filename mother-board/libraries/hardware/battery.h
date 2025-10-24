#ifndef BATTERY_H__
#define BATTERY_H__
#include <utility>
#include <vector>
#include <console.h>
namespace Battery {
    bool initialized;
    std::vector<std::pair<int, float>> mapping;
    void begin() {
        mapping.push_back(std::make_pair(0, 0));
        mapping.push_back(std::make_pair(3100, 0));
        mapping.push_back(std::make_pair(3117, 5));
        mapping.push_back(std::make_pair(3133, 8));
        mapping.push_back(std::make_pair(3149, 10));
        mapping.push_back(std::make_pair(3166, 12));
        mapping.push_back(std::make_pair(3182, 14));
        mapping.push_back(std::make_pair(3198, 15));
        mapping.push_back(std::make_pair(3214, 16));
        mapping.push_back(std::make_pair(3231, 17));
        mapping.push_back(std::make_pair(3247, 18));
        mapping.push_back(std::make_pair(3263, 19));
        mapping.push_back(std::make_pair(3280, 20.4));
        mapping.push_back(std::make_pair(3296, 21.7));
        mapping.push_back(std::make_pair(3312, 23.2));
        mapping.push_back(std::make_pair(3328, 24.7));
        mapping.push_back(std::make_pair(3345, 26.4));
        mapping.push_back(std::make_pair(3361, 28.1));
        mapping.push_back(std::make_pair(3377, 30));
        mapping.push_back(std::make_pair(3394, 31.9));
        mapping.push_back(std::make_pair(3410, 34));
        mapping.push_back(std::make_pair(3426, 36.2));
        mapping.push_back(std::make_pair(3442, 38.6));
        mapping.push_back(std::make_pair(3459, 41));
        mapping.push_back(std::make_pair(3475, 43.7));
        mapping.push_back(std::make_pair(3491, 46.4));
        mapping.push_back(std::make_pair(3508, 49.3));
        mapping.push_back(std::make_pair(3524, 52.4));
        mapping.push_back(std::make_pair(3540, 55.6));
        mapping.push_back(std::make_pair(3557, 58.9));
        mapping.push_back(std::make_pair(3573, 62.4));
        mapping.push_back(std::make_pair(3589, 66));
        mapping.push_back(std::make_pair(3605, 69.7));
        mapping.push_back(std::make_pair(3622, 73.4));
        mapping.push_back(std::make_pair(3638, 77));
        mapping.push_back(std::make_pair(3654, 80.6));
        mapping.push_back(std::make_pair(3671, 83.9));
        mapping.push_back(std::make_pair(3687, 87));
        mapping.push_back(std::make_pair(3703, 89.7));
        mapping.push_back(std::make_pair(3719, 92));
        mapping.push_back(std::make_pair(3736, 93.9));
        mapping.push_back(std::make_pair(3752, 95.4));
        mapping.push_back(std::make_pair(3768, 96.6));
        mapping.push_back(std::make_pair(3785, 97.5));
        mapping.push_back(std::make_pair(3801, 98.2));
        mapping.push_back(std::make_pair(3817, 98.7));
        mapping.push_back(std::make_pair(3833, 99.1));
        mapping.push_back(std::make_pair(3850, 99.3));
        mapping.push_back(std::make_pair(3866, 99.5));
        mapping.push_back(std::make_pair(3882, 99.7));
        mapping.push_back(std::make_pair(3899, 99.7));
        mapping.push_back(std::make_pair(3915, 99.8));
        mapping.push_back(std::make_pair(3931, 99.9));
        mapping.push_back(std::make_pair(3947, 99.9));
        mapping.push_back(std::make_pair(3964, 99.9));
        mapping.push_back(std::make_pair(3980, 100));
    }

    float getPercentage(int input) {
        if (!initialized) {
            initialized = true;
            begin();
        }
        for (int i=1; i<mapping.size(); i++) {
            auto [pMv, pPercent] = mapping[i-1];
            auto [mv, percent] = mapping[i];
            if (input >= pMv && input <= mv) {
                return  (pPercent + percent) / 2;
            }
        }
        return 100;
    }
};
#endif