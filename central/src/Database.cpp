#include "central/Database.h"
#include <algorithm>
#include <fstream>
#include <cstdio>

namespace central {

static uint32_t latestTs(const std::vector<Measurement>& m, const std::vector<EventRec>& e) {
    uint32_t t = 0;
    for (const auto& x : m) t = std::max(t, x.ts);
    for (const auto& x : e) t = std::max(t, x.ts);
    return t;
}

void Database::prune() {
    uint32_t latest = latestTs(meas_, events_);
    if (latest <= WEEK_SECONDS) return;               // nothing old enough to drop
    uint32_t cutoff = latest - WEEK_SECONDS;
    meas_.erase(std::remove_if(meas_.begin(), meas_.end(),
                [cutoff](const Measurement& m){ return m.ts < cutoff; }), meas_.end());
    events_.erase(std::remove_if(events_.begin(), events_.end(),
                [cutoff](const EventRec& e){ return e.ts < cutoff; }), events_.end());
}

void Database::addMeasurement(const Measurement& m) { meas_.push_back(m); prune(); }
void Database::addEvent(const EventRec& e)          { events_.push_back(e); prune(); }

std::vector<Measurement> Database::measurementsInRange(uint32_t from, uint32_t to) const {
    std::vector<Measurement> out;
    for (const auto& m : meas_) if (m.ts >= from && m.ts <= to) out.push_back(m);
    return out;
}
std::vector<EventRec> Database::eventsInRange(uint32_t from, uint32_t to) const {
    std::vector<EventRec> out;
    for (const auto& e : events_) if (e.ts >= from && e.ts <= to) out.push_back(e);
    return out;
}

Database::Summary Database::summarize(uint32_t from, uint32_t to) const {
    Summary s;
    double ts = 0, hs = 0, ls = 0, bs = 0;
    for (const auto& m : meas_) {
        if (m.ts < from || m.ts > to) continue;
        if (s.count == 0) {
            s.tmin = s.tmax = m.temp; s.hmin = s.hmax = m.hum;
            s.lmin = s.lmax = m.light; s.bmin = s.bmax = m.batt;
        } else {
            s.tmin = std::min(s.tmin, m.temp); s.tmax = std::max(s.tmax, m.temp);
            s.hmin = std::min(s.hmin, m.hum);  s.hmax = std::max(s.hmax, m.hum);
            s.lmin = std::min(s.lmin, m.light);s.lmax = std::max(s.lmax, m.light);
            s.bmin = std::min(s.bmin, m.batt); s.bmax = std::max(s.bmax, m.batt);
        }
        ts += m.temp; hs += m.hum; ls += m.light; bs += m.batt;
        if (m.mode == 0) ++s.modeNormal; else if (m.mode == 1) ++s.modeWarning; else ++s.modeError;
        ++s.count;
    }
    if (s.count) { s.tavg = ts/s.count; s.havg = hs/s.count; s.lavg = ls/s.count; s.bavg = bs/s.count; }
    return s;
}

Database::EventStats Database::eventStats(uint32_t from, uint32_t to) const {
    EventStats st;
    for (const auto& e : events_) {
        if (e.ts < from || e.ts > to) continue;
        ++st.total;
        switch (e.src) {
            case 0: ++st.monitor; break;
            case 1: ++st.object; if (e.detected) ++st.objectDetected; break;
            case 2: ++st.config; break;
            case 3: ++st.init; break;
        }
    }
    return st;
}

bool Database::saveCsv(const std::string& dir) const {
    std::ofstream fm(dir + "/measurements.csv");
    if (!fm) return false;
    for (const auto& m : meas_)
        fm << m.ts << ',' << m.temp << ',' << m.hum << ',' << m.light << ',' << m.batt << ',' << (int)m.mode << '\n';
    std::ofstream fe(dir + "/events.csv");
    if (!fe) return false;
    for (const auto& e : events_)
        fe << e.ts << ',' << (int)e.src << ',' << (int)e.from_mode << ',' << (int)e.to_mode << ',' << (e.detected?1:0) << '\n';
    return true;
}

bool Database::loadCsv(const std::string& dir) {
    meas_.clear(); events_.clear();
    std::ifstream fm(dir + "/measurements.csv");
    if (fm) {
        unsigned long ts, hum, light, batt; long temp; int mode; char comma;
        std::string line;
        while (std::getline(fm, line)) {
            Measurement m; int matched = std::sscanf(line.c_str(), "%lu,%ld,%lu,%lu,%lu,%d",
                                                     &ts, &temp, &hum, &light, &batt, &mode);
            if (matched == 6) { m.ts=ts; m.temp=(int16_t)temp; m.hum=(uint16_t)hum;
                                m.light=(uint16_t)light; m.batt=(uint16_t)batt; m.mode=(uint8_t)mode;
                                meas_.push_back(m); }
            (void)comma;
        }
    }
    std::ifstream fe(dir + "/events.csv");
    if (fe) {
        std::string line;
        while (std::getline(fe, line)) {
            unsigned long ts; int src, fm2, to, det;
            if (std::sscanf(line.c_str(), "%lu,%d,%d,%d,%d", &ts, &src, &fm2, &to, &det) == 5) {
                EventRec e; e.ts=ts; e.src=(uint8_t)src; e.from_mode=(uint8_t)fm2;
                e.to_mode=(uint8_t)to; e.detected=(det!=0); events_.push_back(e);
            }
        }
    }
    prune();
    return true;
}

}
