#include "ConfigManager.hpp"
#include <cmath>
#include <cstdlib>
#include <hyprlang.hpp>
#include <hyprutils/path/Path.hpp>
#include <string>
#include <sys/ucontext.h>
#include "helpers/Log.hpp"

static std::string getMainConfigPath() {
    static const auto paths = Hyprutils::Path::findConfig("hyprsunset");

    return paths.first.value_or("");
}

CConfigManager::CConfigManager(std::string configPath) :
    m_config(configPath.empty() ? getMainConfigPath().c_str() : configPath.c_str(), Hyprlang::SConfigOptions{.throwAllErrors = true, .allowMissingConfig = true}) {
    currentConfigPath = configPath.empty() ? getMainConfigPath() : configPath;
}

void CConfigManager::init() {
    m_config.addConfigValue("max-gamma", Hyprlang::INT{100});

    // solar / geolocation
    m_config.addConfigValue("latitude", Hyprlang::FLOAT{std::nanf("")});
    m_config.addConfigValue("longitude", Hyprlang::FLOAT{std::nanf("")});
    m_config.addConfigValue("elevation_twilight", Hyprlang::FLOAT{-6.0f});
    m_config.addConfigValue("elevation_daylight", Hyprlang::FLOAT{3.0f});
    m_config.addConfigValue("transition_step", Hyprlang::INT{10});
    m_config.addConfigValue("high_temperature", Hyprlang::INT{6500});
    m_config.addConfigValue("low_temperature", Hyprlang::INT{4000});

    m_config.addSpecialCategory("profile", Hyprlang::SSpecialCategoryOptions{.key = nullptr, .anonymousKeyBased = true});
    m_config.addSpecialConfigValue("profile", "time", Hyprlang::STRING{"00:00"});
    m_config.addSpecialConfigValue("profile", "temperature", Hyprlang::INT{6000});
    m_config.addSpecialConfigValue("profile", "gamma", Hyprlang::FLOAT{1.0f});
    m_config.addSpecialConfigValue("profile", "identity", Hyprlang::INT{0});

    m_config.commence();

    auto result = m_config.parse();

    if (result.error)
        Debug::log(ERR, "Config has errors:\n{}\nProceeding ignoring faulty entries", result.getError());
}

std::vector<SSunsetProfile> CConfigManager::getSunsetProfiles() {
    std::vector<SSunsetProfile> result;

    auto                        keys = m_config.listKeysForSpecialCategory("profile");
    result.reserve(keys.size());

    for (auto& key : keys) {
        std::string   time;
        unsigned long temperature;
        float         gamma;
        bool          identity;

        try {
            time        = std::any_cast<Hyprlang::STRING>(m_config.getSpecialConfigValue("profile", "time", key.c_str()));
            temperature = std::any_cast<Hyprlang::INT>(m_config.getSpecialConfigValue("profile", "temperature", key.c_str()));
            gamma       = std::any_cast<Hyprlang::FLOAT>(m_config.getSpecialConfigValue("profile", "gamma", key.c_str()));
            identity    = std::any_cast<Hyprlang::INT>(m_config.getSpecialConfigValue("profile", "identity", key.c_str()));
        } catch (const std::bad_any_cast& e) {
            RASSERT(false, "Failed to construct Profile: {}", e.what()); //
        } catch (const std::out_of_range& e) {
            RASSERT(false, "Missing property for Profile: {}", e.what()); //
        }

        size_t separator = time.find(':');

        if (separator == std::string::npos) {
            Debug::log(ERR, "Invalid time format: {}, skipping profile {}", time, key);
            continue;
        }

        int hour = 0, minute = 0;
        try {
            hour   = std::stoi(time.substr(0, separator));
            minute = std::stoi(time.substr(separator + 1).c_str());
        } catch (const std::exception& e) {
            Debug::log(ERR, "Invalid time format: {}, skipping profile {}", time, key);
            continue;
        }

        // clang-format off
        result.push_back(SSunsetProfile{
            .time = {
                .hour   = std::chrono::hours(hour),
                .minute = std::chrono::minutes(minute),
            },
            .temperature = temperature,
            .gamma       = gamma,
            .identity    = identity,
        });
        // clang-format on
    }

    return result;
}

float CConfigManager::getMaxGamma() {
    try {
        return std::any_cast<Hyprlang::INT>(m_config.getConfigValue("max-gamma")) / 100.f;
    } catch (const std::bad_any_cast& e) {
        RASSERT(false, "Failed to construct max-gamma: {}", e.what()); //
    }
}

float CConfigManager::getLatitude() {
    try {
        return std::any_cast<Hyprlang::FLOAT>(m_config.getConfigValue("latitude"));
    } catch (const std::bad_any_cast&) {
        return std::nanf("");
    }
}

float CConfigManager::getLongitude() {
    try {
        return std::any_cast<Hyprlang::FLOAT>(m_config.getConfigValue("longitude"));
    } catch (const std::bad_any_cast&) {
        return std::nanf("");
    }
}

float CConfigManager::getElevationTwilight() {
    try {
        return std::any_cast<Hyprlang::FLOAT>(m_config.getConfigValue("elevation_twilight"));
    } catch (const std::bad_any_cast&) {
        return -6.0f;
    }
}

float CConfigManager::getElevationDaylight() {
    try {
        return std::any_cast<Hyprlang::FLOAT>(m_config.getConfigValue("elevation_daylight"));
    } catch (const std::bad_any_cast&) {
        return 3.0f;
    }
}

int CConfigManager::getTransitionStep() {
    try {
        return std::any_cast<Hyprlang::INT>(m_config.getConfigValue("transition_step"));
    } catch (const std::bad_any_cast&) {
        return 10;
    }
}

int CConfigManager::getHighTemperature() {
    try {
        return std::any_cast<Hyprlang::INT>(m_config.getConfigValue("high_temperature"));
    } catch (const std::bad_any_cast&) {
        return 6500;
    }
}

int CConfigManager::getLowTemperature() {
    try {
        return std::any_cast<Hyprlang::INT>(m_config.getConfigValue("low_temperature"));
    } catch (const std::bad_any_cast&) {
        return 4000;
    }
}
