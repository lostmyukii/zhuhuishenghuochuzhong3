#include "solar_terms.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "smartlife_config.h"

namespace smartlife {
namespace {

const std::array<SolarTermProfile, SOLAR_TERM_COUNT> PROFILES = {{
    {SolarTerm::Lichun, "立春", 26, 20, 35},
    {SolarTerm::Yushui, "雨水", 26, 20, 35},
    {SolarTerm::Jingzhe, "惊蛰", 26, 50, 35},
    {SolarTerm::Chunfen, "春分", 26, 50, 35},
    {SolarTerm::Qingming, "清明", 26, 50, 35},
    {SolarTerm::Guyu, "谷雨", 26, 50, 35},
    {SolarTerm::Lixia, "立夏", 27, 80, 35},
    {SolarTerm::Xiaoman, "小满", 27, 80, 35},
    {SolarTerm::Mangzhong, "芒种", 27, 80, 35},
    {SolarTerm::Xiazhi, "夏至", 27, 80, 30},
    {SolarTerm::Xiaoshu, "小暑", 26, 80, 30},
    {SolarTerm::Dashu, "大暑", 26, 80, 30},
    {SolarTerm::Liqiu, "立秋", 27, 80, 35},
    {SolarTerm::Chushu, "处暑", 27, 80, 35},
    {SolarTerm::Bailu, "白露", 26, 50, 35},
    {SolarTerm::Qiufen, "秋分", 26, 50, 35},
    {SolarTerm::Hanlu, "寒露", 25, 20, 40},
    {SolarTerm::Shuangjiang, "霜降", 25, 20, 40},
    {SolarTerm::Lidong, "立冬", 24, 20, 45},
    {SolarTerm::Xiaoxue, "小雪", 24, 20, 45},
    {SolarTerm::Daxue, "大雪", 24, 20, 45},
    {SolarTerm::Dongzhi, "冬至", 24, 20, 45},
    {SolarTerm::Xiaohan, "小寒", 24, 20, 45},
    {SolarTerm::Dahan, "大寒", 24, 20, 45},
}};

int clampAdcRaw(int rawValue) {
  return std::max(0, std::min(4095, rawValue));
}

}  // namespace

const std::array<SolarTermProfile, SOLAR_TERM_COUNT>& solarTermProfiles() {
  return PROFILES;
}

const SolarTermProfile& solarTermProfile(SolarTerm term) {
  const std::size_t index = static_cast<std::size_t>(term);
  return index < PROFILES.size() ? PROFILES[index] : PROFILES.front();
}

bool solarTermFromName(const char* name, SolarTerm& result) {
  if (name == nullptr) {
    return false;
  }

  for (const SolarTermProfile& profile : PROFILES) {
    if (std::strcmp(name, profile.name) == 0) {
      result = profile.term;
      return true;
    }
  }
  return false;
}

int mapKnobRawToThresholdC(int rawValue) {
  const int clampedRaw = clampAdcRaw(rawValue);
  const int temperatureSpan = TEMPERATURE_THRESHOLD_MAX_C -
                              TEMPERATURE_THRESHOLD_MIN_C;
  return TEMPERATURE_THRESHOLD_MIN_C +
         (clampedRaw * temperatureSpan + 2047) / 4095;
}

int medianOfFiveRaw(const std::array<int, 5>& samples) {
  std::array<int, 5> sorted = samples;
  std::sort(sorted.begin(), sorted.end());
  return clampAdcRaw(sorted[2]);
}

int applyKnobDeadband(int lastAcceptedRaw,
                      int candidateRaw,
                      int deadbandRaw) {
  if (candidateRaw <= 0) {
    return 0;
  }
  if (candidateRaw >= 4095) {
    return 4095;
  }

  const int last = clampAdcRaw(lastAcceptedRaw);
  const int candidate = clampAdcRaw(candidateRaw);
  const int deadband = std::max(0, deadbandRaw);
  return std::abs(candidate - last) < deadband ? last : candidate;
}

}  // namespace smartlife
