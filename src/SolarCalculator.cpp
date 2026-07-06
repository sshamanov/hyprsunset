#include "SolarCalculator.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace SolarCalculator {

    static constexpr double PI         = std::numbers::pi;
    static constexpr double RAD_TO_DEG = 180.0 / PI;
    static constexpr double DEG_TO_RAD = PI / 180.0;

    // ---- solar geometry (NOAA equations) ----------------------------------

    static int daysInYear(int year) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        return leap ? 366 : 365;
    }

    static double dateOrbitAngle(const std::tm& tm) {
        return 2.0 * PI / static_cast<double>(daysInYear(tm.tm_year + 1900)) *
               static_cast<double>(tm.tm_yday);
    }

    static double equationOfTime(double orbitAngle) {
        return 4.0 * (0.000075 + 0.001868 * std::cos(orbitAngle) -
                      0.032077 * std::sin(orbitAngle) -
                      0.014615 * std::cos(2.0 * orbitAngle) -
                      0.040849 * std::sin(2.0 * orbitAngle));
    }

    static double sunDeclination(double orbitAngle) {
        return 0.006918 - 0.399912 * std::cos(orbitAngle) +
               0.070257 * std::sin(orbitAngle) -
               0.006758 * std::cos(2.0 * orbitAngle) +
               0.000907 * std::sin(2.0 * orbitAngle) -
               0.002697 * std::cos(3.0 * orbitAngle) +
               0.00148 * std::sin(3.0 * orbitAngle);
    }

    static double sunHourAngle(double latitude, double declination,
                               double targetSunZenith) {
        return std::acos(
            targetSunZenith / (std::cos(latitude) * std::cos(declination)) -
            std::tan(latitude) * std::tan(declination));
    }

    static time_t hourAngleToTime(double hourAngle, double eqtime) {
        return static_cast<time_t>(
            (4.0 * PI - 4.0 * hourAngle - eqtime) * 60.0 * RAD_TO_DEG);
    }

    static SunCondition polarCondition(double latitudeRad,
                                       double sunDeclinationVal) {
        bool sameSign = (std::signbit(latitudeRad) ==
                         std::signbit(sunDeclinationVal));
        return sameSign ? SunCondition::MidnightSun : SunCondition::PolarNight;
    }

    SunCondition calculateSolarTimes(const std::tm& tm, double latitude,
                                     double elevationTwilightRad,
                                     double elevationDaylightRad,
                                     SolarTimes&     outTimes) {
        double orbitAngle = dateOrbitAngle(tm);
        double decl       = sunDeclination(orbitAngle);
        double eqtime     = equationOfTime(orbitAngle);

        double haTwilight = sunHourAngle(latitude, decl, elevationTwilightRad);
        double haDaylight = sunHourAngle(latitude, decl, elevationDaylightRad);

        outTimes.dawn    = hourAngleToTime(std::abs(haTwilight), eqtime);
        outTimes.night   = hourAngleToTime(-std::abs(haTwilight), eqtime);
        outTimes.sunrise = hourAngleToTime(std::abs(haDaylight), eqtime);
        outTimes.sunset  = hourAngleToTime(-std::abs(haDaylight), eqtime);

        if (std::isnan(haTwilight) || std::isnan(haDaylight))
            return polarCondition(latitude, decl);
        return SunCondition::Normal;
    }

    // ---- colour science ----------------------------------------------------

    namespace {
        struct XYZ { double x, y, z; };

        bool illuminantD(int temp, double& x, double& y) {
            double t = static_cast<double>(temp);
            if (temp < 2500) return false;
            if (temp <= 7000) {
                x = 0.244063 + 0.09911e3 / t + 2.9678e6 / (t * t) -
                    4.6070e9 / (t * t * t);
            } else if (temp <= 25000) {
                x = 0.237040 + 0.24748e3 / t + 1.9018e6 / (t * t) -
                    2.0064e9 / (t * t * t);
            } else {
                x = 0.237040 + 0.24748e3 / 25000.0 +
                    1.9018e6 / (25000.0 * 25000.0) -
                    2.0064e9 / (25000.0 * 25000.0 * 25000.0);
            }
            y = -3.0 * x * x + 2.870 * x - 0.275;
            return true;
        }

        bool planckianLocus(int temp, double& x, double& y) {
            double t = static_cast<double>(temp);
            if (temp < 1667) return false;
            if (temp <= 4000) {
                x = -0.2661239e9 / (t * t * t) -
                    0.2343589e6 / (t * t) + 0.8776956e3 / t + 0.179910;
                if (temp <= 2222)
                    y = -1.1064814 * x * x * x - 1.34811020 * x * x +
                        2.18555832 * x - 0.20219683;
                else
                    y = -0.9549476 * x * x * x - 1.37418593 * x * x +
                        2.09137015 * x - 0.16748867;
            } else if (temp < 25000) {
                x = -3.0258469e9 / (t * t * t) + 2.1070379e6 / (t * t) +
                    0.2226347e3 / t + 0.240390;
                y = 3.0817580 * x * x * x - 5.87338670 * x * x +
                    3.75112997 * x - 0.37001483;
            } else {
                x = -3.0258469e9 / (25000.0 * 25000.0 * 25000.0) +
                    2.1070379e6 / (25000.0 * 25000.0) +
                    0.2226347e3 / 25000.0 + 0.240390;
                y = 3.0817580 * x * x * x - 5.87338670 * x * x +
                    3.75112997 * x - 0.37001483;
            }
            return true;
        }

        double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

        XYZ xyYtoXYZ(double x, double y) { return {x, y, 1.0 - x - y}; }

        Whitepoint xyzToLinearSRGB(const XYZ& xyz) {
            return {
                std::pow(clamp01(3.2404542 * xyz.x - 1.5371385 * xyz.y -
                                 0.4985314 * xyz.z), 1.0 / 2.2),
                std::pow(clamp01(-0.9692660 * xyz.x + 1.8760108 * xyz.y +
                                 0.0415560 * xyz.z), 1.0 / 2.2),
                std::pow(clamp01(0.0556434 * xyz.x - 0.2040259 * xyz.y +
                                 1.0572252 * xyz.z), 1.0 / 2.2),
            };
        }

        void normaliseWhitepoint(Whitepoint& wp) {
            double maxCh = std::max({wp.r, wp.g, wp.b});
            wp.r /= maxCh; wp.g /= maxCh; wp.b /= maxCh;
        }
    } // anonymous namespace

    Whitepoint calculateWhitepoint(int temp) {
        if (temp == 6500) return {1.0, 1.0, 1.0};

        double x, y;
        if (temp >= 25000) {
            illuminantD(25000, x, y);
        } else if (temp >= 4000) {
            illuminantD(temp, x, y);
        } else if (temp >= 2500) {
            double xD, yD, xP, yP;
            illuminantD(temp, xD, yD);
            planckianLocus(temp, xP, yP);
            double factor     = (4000.0 - temp) / 1500.0;
            double sineFactor = (std::cos(PI * factor) + 1.0) / 2.0;
            x = xD * sineFactor + xP * (1.0 - sineFactor);
            y = yD * sineFactor + yP * (1.0 - sineFactor);
        } else {
            planckianLocus(temp >= 1667 ? temp : 1667, x, y);
        }

        XYZ xyz = xyYtoXYZ(x, y);
        Whitepoint wp = xyzToLinearSRGB(xyz);
        normaliseWhitepoint(wp);
        return wp;
    }

} // namespace SolarCalculator
