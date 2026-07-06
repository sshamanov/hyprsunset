#pragma once

#include "Hyprsunset.hpp"
#include <hyprlang.hpp>
#include <vector>

class CConfigManager {
  public:
    CConfigManager(std::string configPath);

    std::vector<SSunsetProfile> getSunsetProfiles();
    float                       getMaxGamma();

    // solar / geolocation config
    float getLatitude();          // returns radians, or NAN if unset
    float getLongitude();         // returns radians, or NAN if unset
    float getElevationTwilight(); // degrees
    float getElevationDaylight(); // degrees
    int   getTransitionStep();    // kelvin per step
    int   getHighTemperature();   // K
    int   getLowTemperature();    // K

    void  init();

  private:
    Hyprlang::CConfig m_config;

    std::string       currentConfigPath;
};

inline UP<CConfigManager> g_pConfigManager;
