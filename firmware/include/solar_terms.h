#pragma once

#include <array>
#include <cstddef>

namespace smartlife {

enum class SolarTerm : unsigned char {
  Lichun,
  Yushui,
  Jingzhe,
  Chunfen,
  Qingming,
  Guyu,
  Lixia,
  Xiaoman,
  Mangzhong,
  Xiazhi,
  Xiaoshu,
  Dashu,
  Liqiu,
  Chushu,
  Bailu,
  Qiufen,
  Hanlu,
  Shuangjiang,
  Lidong,
  Xiaoxue,
  Daxue,
  Dongzhi,
  Xiaohan,
  Dahan,
};

constexpr std::size_t SOLAR_TERM_COUNT = 24;

struct SolarTermProfile {
  SolarTerm term;
  const char* name;
  int recommendedTemperatureC;
  int curtainClosePercent;
  int lightThreshold;
};

const std::array<SolarTermProfile, SOLAR_TERM_COUNT>& solarTermProfiles();
const SolarTermProfile& solarTermProfile(SolarTerm term);
bool solarTermFromName(const char* name, SolarTerm& result);

int mapKnobRawToThresholdC(int rawValue);
int medianOfFiveRaw(const std::array<int, 5>& samples);
int applyKnobDeadband(int lastAcceptedRaw,
                      int candidateRaw,
                      int deadbandRaw = 16);

}  // namespace smartlife
