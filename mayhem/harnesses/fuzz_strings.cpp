// Small string grammars: unit expressions (Units, UnitSystem::convert,
// activation-energy conversion), composition strings (parseCompString) and the
// float parser (fpValueCheck). The input is split on newlines; consecutive
// lines are used as (source, destination) pairs.
#include "fuzz_common.h"
#include "cantera/base/Units.h"
#include "cantera/base/stringUtils.h"
#include <vector>

using namespace Cantera;

extern "C" int LLVMFuzzerInitialize(int*, char***) { fuzzcommon::init(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string s = fuzzcommon::str(data, size);
    vector<string> lines;
    size_t start = 0;
    while (start <= s.size() && lines.size() < 16) {
        size_t nl = s.find('\n', start);
        if (nl == string::npos) nl = s.size();
        lines.push_back(s.substr(start, nl - start));
        start = nl + 1;
    }
    UnitSystem us;
    for (size_t i = 0; i < lines.size(); i++) {
        const string& a = lines[i];
        const string& b = lines[(i + 1) % lines.size()];
        try { Units u(a); (void)u.str(); (void)u.factor(); } catch (const std::exception&) {}
        try { (void)us.convert(1.5, a, b); } catch (const std::exception&) {}
        try { (void)us.convertActivationEnergy(2.5, a, b); } catch (const std::exception&) {}
        try { (void)parseCompString(a); } catch (const std::exception&) {}
        try { (void)parseCompString(a, {"H2", "O2", "N2", "AR"}); } catch (const std::exception&) {}
        try { (void)fpValueCheck(a); } catch (const std::exception&) {}
    }
    return 0;
}
