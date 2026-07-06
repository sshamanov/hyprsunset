#include "ConfigManager.hpp"
#include "helpers/Log.hpp"

#include <cmath>
#include <numbers>

static void printHelp() {
    Debug::log(NONE, "┣ --gamma              -g  →  Set the display gamma (default 100%)");
    Debug::log(NONE, "┣ --gamma_max              →  Set the maximum display gamma (default 100%, maximum 200%)");
    Debug::log(NONE, "┣ --temperature        -t  →  Set the temperature in K (default 6000)");
    Debug::log(NONE, "┣ --identity           -i  →  Use the identity matrix (no color change)");
    Debug::log(NONE, "┣ --verbose                →  Print more logging");
    Debug::log(NONE, "┣ --version            -v  →  Print the version");
    Debug::log(NONE, "┣ --help               -h  →  Print this info");
    Debug::log(NONE, "┣                                       ━━ solar / geolocation mode ━━");
    Debug::log(NONE, "┣ --latitude           -l  →  Set latitude in degrees (e.g. 59.9)");
    Debug::log(NONE, "┣ --longitude          -L  →  Set longitude in degrees (e.g. 30.3)");
    Debug::log(NONE, "┣ --elevation-daylight     →  Solar elevation for daylight transition (default 3.0°)");
    Debug::log(NONE, "┣ --elevation-twilight     →  Solar elevation for twilight transition (default -6.0°)");
    Debug::log(NONE, "┣ --transition-step        →  Kelvin per animation step (default 10)");
    Debug::log(NONE, "╹");
}

int main(int argc, char** argv, char** envp) {
    std::string configPath;

    int         kelvin   = -1;
    float       gamma    = -1;
    float       maxGamma = -1;
    bool        identity = false;

    // solar / geolocation
    float latArg          = std::nanf("");
    float longArg         = std::nanf("");
    float elevDayArg      = std::nanf("");
    float elevTwilightArg = std::nanf("");
    int   transStepArg    = -1;
    int   highTempArg     = -1;
    int   lowTempArg      = -1;

    g_pHyprsunset = std::make_unique<CHyprsunset>();

    for (int i = 1; i < argc; ++i) {
        if (argv[i] == std::string{"-t"} || argv[i] == std::string{"--temperature"}) {
            if (i + 1 >= argc) {
                Debug::log(NONE, "✖ No temperature provided for {}", argv[i]);
                return 1;
            }
            try {
                kelvin = std::stoull(argv[i + 1]);
            } catch (std::exception& e) {
                Debug::log(NONE, "✖ Temperature {} is not valid", argv[i + 1]);
                return 1;
            }
            ++i;
        } else if (argv[i] == std::string{"-g"} || argv[i] == std::string{"--gamma"}) {
            if (i + 1 >= argc) {
                Debug::log(NONE, "✖ No gamma provided for {}", argv[i]);
                return 1;
            }
            try {
                gamma = std::stof(argv[i + 1]) / 100;
            } catch (std::exception& e) {
                Debug::log(NONE, "✖ Gamma {} is not valid", argv[i + 1]);
                return 1;
            }
            ++i;
        } else if (argv[i] == std::string{"--gamma_max"}) {
            if (i + 1 >= argc) {
                Debug::log(NONE, "✖ No gamma provided for {}", argv[i]);
                return 1;
            }
            try {
                maxGamma = std::stof(argv[i + 1]) / 100;
            } catch (std::exception& e) {
                Debug::log(NONE, "✖ Maximum gamma {} is not valid", argv[i + 1]);
                return 1;
            }
            ++i;
        } else if (argv[i] == std::string{"-i"} || argv[i] == std::string{"--identity"}) {
            identity = true;
        } else if (argv[i] == std::string{"-c"} || argv[i] == std::string{"--config"}) {
            if (i + 1 >= argc) {
                Debug::log(NONE, "✖ No config path provided for {}", argv[i]);
                return 1;
            }
            configPath = argv[i + 1];
        } else if (argv[i] == std::string{"-h"} || argv[i] == std::string{"--help"}) {
            printHelp();
            return 0;
        } else if (argv[i] == std::string{"-v"} || argv[i] == std::string{"--version"}) {
            Debug::log(NONE, "hyprsunset v{}", HYPRSUNSET_VERSION);
            return 0;
        } else if (argv[i] == std::string{"--verbose"}) {
            Debug::trace = true;
        } else if (argv[i] == std::string{"-l"} || argv[i] == std::string{"--latitude"}) {
            if (i + 1 >= argc) {
                Debug::log(NONE, "✖ No latitude provided for {}", argv[i]);
                return 1;
            }
            try {
                latArg = std::stof(argv[i + 1]);
            } catch (std::exception& e) {
                Debug::log(NONE, "✖ Latitude {} is not valid", argv[i + 1]);
                return 1;
            }
            ++i;
        } else if (argv[i] == std::string{"-L"} || argv[i] == std::string{"--longitude"}) {
            if (i + 1 >= argc) {
                Debug::log(NONE, "✖ No longitude provided for {}", argv[i]);
                return 1;
            }
            try {
                longArg = std::stof(argv[i + 1]);
            } catch (std::exception& e) {
                Debug::log(NONE, "✖ Longitude {} is not valid", argv[i + 1]);
                return 1;
            }
            ++i;
        } else if (argv[i] == std::string{"--elevation-daylight"}) {
            if (i + 1 >= argc) {
                Debug::log(NONE, "✖ No elevation provided for {}", argv[i]);
                return 1;
            }
            try {
                elevDayArg = std::stof(argv[i + 1]);
            } catch (std::exception& e) {
                Debug::log(NONE, "✖ Elevation {} is not valid", argv[i + 1]);
                return 1;
            }
            ++i;
        } else if (argv[i] == std::string{"--elevation-twilight"}) {
            if (i + 1 >= argc) {
                Debug::log(NONE, "✖ No elevation provided for {}", argv[i]);
                return 1;
            }
            try {
                elevTwilightArg = std::stof(argv[i + 1]);
            } catch (std::exception& e) {
                Debug::log(NONE, "✖ Elevation {} is not valid", argv[i + 1]);
                return 1;
            }
            ++i;
        } else if (argv[i] == std::string{"--transition-step"}) {
            if (i + 1 >= argc) {
                Debug::log(NONE, "✖ No step provided for {}", argv[i]);
                return 1;
            }
            try {
                transStepArg = std::stoi(argv[i + 1]);
            } catch (std::exception& e) {
                Debug::log(NONE, "✖ Transition step {} is not valid", argv[i + 1]);
                return 1;
            }
            ++i;
        } else {
            Debug::log(NONE, "✖ Argument not recognized: {}", argv[i]);
            printHelp();
            return 1;
        }
    }

    Debug::log(NONE, "┏ hyprsunset v{} ━━╸\n┃", HYPRSUNSET_VERSION);

    g_pConfigManager = makeUnique<CConfigManager>(configPath);
    g_pConfigManager->init();

    // ---- apply solar / geolocation config ----
    {
        float cfgLat      = g_pConfigManager->getLatitude();
        float cfgLong     = g_pConfigManager->getLongitude();
        float cfgElevDay  = g_pConfigManager->getElevationDaylight();
        float cfgElevTw   = g_pConfigManager->getElevationTwilight();
        int   cfgTransSt  = g_pConfigManager->getTransitionStep();
        int   cfgHighTemp = g_pConfigManager->getHighTemperature();
        int   cfgLowTemp  = g_pConfigManager->getLowTemperature();

        float finalLat  = !std::isnan(latArg) ? latArg : cfgLat;
        float finalLong = !std::isnan(longArg) ? longArg : cfgLong;
        g_pHyprsunset->SOLAR_ELEVATION_DAYLIGHT = !std::isnan(elevDayArg) ? elevDayArg : cfgElevDay;
        g_pHyprsunset->SOLAR_ELEVATION_TWILIGHT = !std::isnan(elevTwilightArg) ? elevTwilightArg : cfgElevTw;
        g_pHyprsunset->SOLAR_ANIM_STEP = transStepArg > 0 ? transStepArg : cfgTransSt;
        g_pHyprsunset->HIGH_KELVIN     = highTempArg > 0 ? highTempArg : cfgHighTemp;
        g_pHyprsunset->LOW_KELVIN      = lowTempArg > 0 ? lowTempArg : cfgLowTemp;

        if (!std::isnan(finalLat) && !std::isnan(finalLong)) {
            if (finalLat > 90.0f || finalLat < -90.0f) {
                Debug::log(NONE, "✖ Latitude ({}) must be in [-90, 90]", finalLat);
                return 1;
            }
            if (finalLong > 180.0f || finalLong < -180.0f) {
                Debug::log(NONE, "✖ Longitude ({}) must be in [-180, 180]", finalLong);
                return 1;
            }
            g_pHyprsunset->solarMode = true;
            g_pHyprsunset->LATITUDE  = static_cast<float>(finalLat * std::numbers::pi_v<float> / 180.0f);
            g_pHyprsunset->LONGITUDE = static_cast<float>(finalLong * std::numbers::pi_v<float> / 180.0f);

            g_pHyprsunset->solarState.longitudeOffset =
                static_cast<time_t>(-g_pHyprsunset->LONGITUDE * 43200.0 / std::numbers::pi);

            Debug::log(NONE, "┣ Solar mode: lat={:.1f}° long={:.1f}°  high={}K  low={}K  step={}K",
                       finalLat, finalLong, g_pHyprsunset->HIGH_KELVIN,
                       g_pHyprsunset->LOW_KELVIN, g_pHyprsunset->SOLAR_ANIM_STEP);
        }
    }

    g_pHyprsunset->loadCurrentProfile();

    if (kelvin != -1) {
        g_pHyprsunset->KELVIN    = kelvin;
        g_pHyprsunset->kelvinSet = true;
        g_pHyprsunset->identity  = false;
    }

    if (gamma != -1)
        g_pHyprsunset->GAMMA = gamma;

    if (maxGamma != -1)
        g_pHyprsunset->MAX_GAMMA = maxGamma;

    if (identity)
        g_pHyprsunset->identity = true;

    if (!g_pHyprsunset->calculateMatrix())
        return 1;
    if (!g_pHyprsunset->init())
        return 1;

    return 0;
}
