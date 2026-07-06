#pragma once

#include <chrono>
#include <ctime>

namespace SolarCalculator {

    enum class SunCondition {
        Normal,
        MidnightSun,
        PolarNight,
    };

    // All offsets are seconds from midnight (0–86400).
    struct SolarTimes {
        time_t dawn    = 0; // start of low→high transition
        time_t sunrise = 0; // end of low→high transition
        time_t sunset  = 0; // start of high→low transition
        time_t night   = 0; // end of high→low transition
    };

    // Linear sRGB whitepoint (normalised so max channel = 1.0).
    struct Whitepoint {
        double r = 1.0, g = 1.0, b = 1.0;
    };

    //
    // Solar geometry
    //

    // Compute today's solar event times (seconds from midnight) for the given
    // date, latitude, and solar elevation thresholds.  Returns the sun condition
    // (Normal, MidnightSun, or PolarNight).
    //
    // latitude               – in radians
    // elevation_twilight_rad – zenith angle of the "twilight" boundary (radians)
    // elevation_daylight_rad – zenith angle of the "daylight" boundary (radians)
    SunCondition calculateSolarTimes(const std::tm& tm,
                                     double          latitude,
                                     double          elevationTwilightRad,
                                     double          elevationDaylightRad,
                                     SolarTimes&     outTimes);

    //
    // Colour science
    //

    // Compute the normalised linear-sRGB whitepoint for a given colour
    // temperature.  Uses a smooth blend of the Planckian locus (dim / warm) and
    // the CIE Illuminant D series (bright / cool).
    //
    // kelvin – colour temperature in Kelvin (1667–25000; clamped at extremes)
    Whitepoint calculateWhitepoint(int kelvin);

} // namespace SolarCalculator
