#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include "central/records.h"

namespace central {

// Data Collection & Analysis (spec §3.4): stores measurements + events, keeps
// only the last 7 days, and produces reports broken down by various criteria.
class Database {
public:
    static constexpr uint32_t WEEK_SECONDS = 7u * 24u * 3600u;

    void addMeasurement(const Measurement& m);
    void addEvent(const EventRec& e);

    const std::vector<Measurement>& measurements() const { return meas_; }
    const std::vector<EventRec>&    events() const       { return events_; }

    std::vector<Measurement> measurementsInRange(uint32_t from, uint32_t to) const;
    std::vector<EventRec>    eventsInRange(uint32_t from, uint32_t to) const;

    struct Summary {
        std::size_t count = 0;
        int16_t  tmin = 0, tmax = 0; double tavg = 0;
        uint16_t hmin = 0, hmax = 0; double havg = 0;
        uint16_t lmin = 0, lmax = 0; double lavg = 0;
        uint16_t bmin = 0, bmax = 0; double bavg = 0;
        std::size_t modeNormal = 0, modeWarning = 0, modeError = 0;
    };
    Summary summarize(uint32_t from, uint32_t to) const;

    struct EventStats {
        std::size_t total = 0;
        std::size_t monitor = 0, object = 0, config = 0, init = 0;
        std::size_t objectDetected = 0;
    };
    EventStats eventStats(uint32_t from, uint32_t to) const;

    // Persistence — the "database" on disk (CSV files in `dir`).
    bool saveCsv(const std::string& dir) const;
    bool loadCsv(const std::string& dir);

private:
    void prune();   // enforce the 7-day window relative to the latest timestamp
    std::vector<Measurement> meas_;
    std::vector<EventRec>    events_;
};

}
