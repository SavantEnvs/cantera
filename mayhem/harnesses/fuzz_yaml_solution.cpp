// Full mechanism load: YAML text -> AnyMap -> newSolution() for every phase
// (thermo, species, kinetics/reaction rate parsers, transport factories), then
// evaluate a few properties so the constructed models are exercised.
#include "fuzz_common.h"
#include "cantera/base/AnyMap.h"
#include "cantera/base/Solution.h"
#include "cantera/thermo/ThermoPhase.h"
#include "cantera/kinetics/Kinetics.h"
#include "cantera/transport/Transport.h"
#include <vector>

using namespace Cantera;

extern "C" int LLVMFuzzerInitialize(int*, char***) { fuzzcommon::init(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    try {
        AnyMap root = AnyMap::fromYamlString(fuzzcommon::str(data, size));
        if (!root.hasKey("phases")) {
            return 0;
        }
        for (auto& ph : root["phases"].asVector<AnyMap>()) {
            try {
                auto sol = newSolution(ph, root);
                auto thermo = sol->thermo();
                thermo->report(true);
                (void)thermo->enthalpy_mass();
                (void)thermo->entropy_mole();
                (void)thermo->cp_mass();
                auto kin = sol->kinetics();
                if (kin && kin->nReactions()) {
                    std::vector<double> kf(kin->nReactions());
                    kin->getFwdRateConstants(kf);
                    std::vector<double> wdot(kin->nTotalSpecies());
                    kin->getNetProductionRates(wdot);
                }
                auto tr = sol->transport();
                if (tr && tr->transportModel() != "none") {
                    (void)tr->viscosity();
                    (void)tr->thermalConductivity();
                }
                (void)sol->parameters(true);
            } catch (const std::exception&) {
            }
        }
    } catch (const std::exception&) {
    }
    return 0;
}
