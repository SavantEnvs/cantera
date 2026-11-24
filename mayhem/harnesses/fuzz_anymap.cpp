// AnyMap YAML parser / converter / emitter: parse, convert every value through
// the typed accessors and unit conversion, serialize with toYamlString() and
// re-parse the emitted text.
#include "fuzz_common.h"
#include "cantera/base/AnyMap.h"
#include "cantera/base/Units.h"

using namespace Cantera;

extern "C" int LLVMFuzzerInitialize(int*, char***) { fuzzcommon::init(); return 0; }

static void walk(const AnyMap& m, int depth) {
    if (depth > 8) return;
    UnitSystem us;
    for (const auto& [key, value] : m) {
        try { (void)value.type_str(); } catch (const std::exception&) {}
        try { if (value.is<AnyMap>()) walk(value.as<AnyMap>(), depth + 1); } catch (const std::exception&) {}
        try { if (value.isVector<AnyMap>()) for (auto& sub : value.asVector<AnyMap>()) walk(sub, depth + 1); } catch (const std::exception&) {}
        try { (void)value.asVector<double>(); } catch (const std::exception&) {}
        try { (void)value.asVector<string>(); } catch (const std::exception&) {}
        try { (void)value.asVector<vector<double>>(); } catch (const std::exception&) {}
        try { (void)value.asMap<double>(); } catch (const std::exception&) {}
        try { (void)value.asDouble(); } catch (const std::exception&) {}
        try { (void)value.asInt(); } catch (const std::exception&) {}
        try { (void)value.asBool(); } catch (const std::exception&) {}
        try { (void)m.convert(key, "m/s"); } catch (const std::exception&) {}
        try { (void)us.convertActivationEnergy(value, "J/kmol"); } catch (const std::exception&) {}
        try { (void)m.convertVector(key, "kg/m^3"); } catch (const std::exception&) {}
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    try {
        AnyMap root = AnyMap::fromYamlString(fuzzcommon::str(data, size));
        walk(root, 0);
        string out = root.toYamlString();
        AnyMap again = AnyMap::fromYamlString(out);
        (void)again.toYamlString();
    } catch (const std::exception&) {
    }
    return 0;
}
