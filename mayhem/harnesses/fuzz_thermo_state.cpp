// ThermoPhase state setters from untrusted text on an H2/O2 gas: the first byte
// picks the equilibrium constraint pair; the rest is a YAML document whose
// "state" map goes to ThermoPhase::setState(AnyMap) and whose "fuel"/"oxidizer"
// strings and "phi" number go to setEquivalenceRatio(); then equilibrate().
#include "fuzz_common.h"
#include "cantera/base/AnyMap.h"
#include "cantera/base/Solution.h"
#include "cantera/thermo/ThermoPhase.h"
#include <vector>

using namespace Cantera;

static shared_ptr<Solution> g_sol;
static vector<double> g_state;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
    fuzzcommon::init();
    g_sol = newSolution("h2o2.yaml", "", "none");
    g_state.resize(g_sol->thermo()->stateSize());
    g_sol->thermo()->saveState(g_state);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return -1;
    }
    static const char* XY[] = {"TP", "HP", "SP", "UV", "SV", "TV"};
    static const char* SOLVER[] = {"auto", "element_potential", "gibbs", "vcs"};
    const char* xy = XY[data[0] % 6];
    const char* solver = SOLVER[(data[0] / 6) % 4];
    auto thermo = g_sol->thermo();
    thermo->restoreState(g_state);
    try {
        AnyMap doc = AnyMap::fromYamlString(fuzzcommon::str(data + 1, size - 1));
        doc.applyUnits();
        if (doc.hasKey("state") && doc["state"].is<AnyMap>()) {
            try { thermo->setState(doc["state"].as<AnyMap>()); } catch (const std::exception&) {}
        }
        if (doc.hasKey("fuel") && doc.hasKey("oxidizer")) {
            double phi = doc.getDouble("phi", 1.0);
            try {
                thermo->setEquivalenceRatio(phi, doc["fuel"].asString(),
                                            doc["oxidizer"].asString());
                (void)thermo->equivalenceRatio();
                (void)thermo->mixtureFraction(doc["fuel"].asString(),
                                              doc["oxidizer"].asString());
            } catch (const std::exception&) {}
        }
        try {
            thermo->equilibrate(xy, solver, 1e-9, 1000, 50);
        } catch (const std::exception&) {}
        (void)thermo->report(true);
    } catch (const std::exception&) {
    }
    thermo->restoreState(g_state);
    return 0;
}
