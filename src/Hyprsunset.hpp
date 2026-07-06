#include "SolarCalculator.hpp"
#include "protocols/hyprland-ctm-control-v1.hpp"
#include "protocols/wayland.hpp"

#include <hyprutils/math/Mat3x3.hpp>
#include <hyprutils/memory/WeakPtr.hpp>

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <sys/signal.h>
#include <vector>
#include <wayland-client.h>
using namespace Hyprutils::Math;
using namespace Hyprutils::Memory;
#define UP CUniquePointer
#define SP CSharedPointer
#define WP CWeakPointer

struct SOutput {
    SP<CCWlOutput> output;
    uint32_t       id = 0;
    void           applyCTM(struct SState*);
};

struct SState {
    SP<CCWlRegistry>                  pRegistry;
    SP<CCHyprlandCtmControlManagerV1> pCTMMgr;
    wl_display*                       wlDisplay = nullptr;
    std::vector<SP<SOutput>>          outputs;
    bool                              initialized = false;
    Mat3x3                            ctm;
    int                               timerFD = -1;
};

struct SSunsetProfile {
    struct {
        std::chrono::hours   hour;
        std::chrono::minutes minute;
    } time;

    unsigned long temperature = 6000;
    float         gamma       = 1.0f;
    bool          identity    = false;
};

// Solar-mode per-day state (analogous to wlsunset's struct context fields).
struct SSolarState {
    time_t dawn    = 0; // absolute UNIX timestamps for today's solar events
    time_t sunrise = 0;
    time_t sunset  = 0;
    time_t night   = 0;

    time_t dawnStep  = 60; // animation step intervals (seconds)
    time_t nightStep = 60;

    time_t calcDay = 0; // midnight of the day the above were calculated for

    SolarCalculator::SunCondition condition = SolarCalculator::SunCondition::Normal;

    time_t longitudeOffset = 0; // cached seconds offset
};

class CHyprsunset {
  public:
    float              MAX_GAMMA = 1.0f; // default
    float              GAMMA     = 1.0f; // default
    unsigned long long KELVIN    = 6000; // default
    bool               kelvinSet = false, identity = false;
    SState             state;
    bool               m_bTerminate = false;

    // ---- solar (geolocation) mode ----
    bool       solarMode                = false;
    int        HIGH_KELVIN              = 6500;  // day temperature (K)
    int        LOW_KELVIN               = 4000;  // night temperature (K)
    float      SOLAR_ELEVATION_TWILIGHT = -6.0f; // degrees (civil twilight)
    float      SOLAR_ELEVATION_DAYLIGHT = 3.0f;  // degrees
    float      LATITUDE                 = 0.0f;  // radians
    float      LONGITUDE                = 0.0f;  // radians
    int        SOLAR_ANIM_STEP          = 10;    // kelvin per animation tick
    SSolarState solarState;

    int                           calculateMatrix();
    int                           init();
    void                          tick();
    void                          loadCurrentProfile();
    std::optional<SSunsetProfile> getCurrentProfile();
    void                          terminate();

    struct {
        std::condition_variable loopSignal;
        std::mutex              loopMutex;
        std::mutex              loopRequestMutex;

        bool                    shouldProcess = false;
        bool                    isScheduled   = false;
    } m_sEventLoopInternals;

  private:
    static void                 commitCTMs();
    void                        reload();
    void                        schedule();
    void                        scheduleSolar();
    int                         currentProfile();
    void                        startEventLoop();

    // solar helpers
    void   recalcSolarStops(time_t now);
    double getSolarPosition(time_t now) const;
    time_t getSolarDeadline(time_t now) const;

    std::vector<SSunsetProfile> profiles;
};

inline std::unique_ptr<CHyprsunset> g_pHyprsunset;
