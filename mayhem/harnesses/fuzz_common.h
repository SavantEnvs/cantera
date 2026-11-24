// Shared setup for the Cantera harnesses: quiet logger, data directory for
// "h2o2.yaml/species"-style references, warnings off (global settings set once,
// read-only afterwards).
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include "cantera/base/global.h"
#include "cantera/base/logger.h"

namespace fuzzcommon {
class NullLogger : public Cantera::Logger {
public:
    void write(const std::string&) override {}
    void writeendl() override {}
    void warn(const std::string&, const std::string&) override {}
};
inline void init() {
    Cantera::setLogger(std::make_unique<NullLogger>());
    Cantera::suppress_deprecation_warnings();
    Cantera::suppress_thermo_warnings(true);
    Cantera::addDataDirectory("/mayhem/data");
    Cantera::addDataDirectory("/mayhem/test/data");
}
inline std::string str(const uint8_t* d, size_t n) {
    return std::string(reinterpret_cast<const char*>(d), n);
}
}
