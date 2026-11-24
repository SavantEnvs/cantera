// Species / species-thermo / transport-data parsers: the input is a YAML list of
// species entries; each goes through newSpecies(), its thermo parameterization
// is evaluated at several temperatures, and the species are added to a fresh
// ideal-gas phase whose properties are then computed.
#include "fuzz_common.h"
#include "cantera/base/AnyMap.h"
#include "cantera/thermo/IdealGasPhase.h"
#include "cantera/thermo/Species.h"
#include "cantera/thermo/SpeciesThermoInterpType.h"
#include <vector>

using namespace Cantera;

extern "C" int LLVMFuzzerInitialize(int*, char***) { fuzzcommon::init(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    try {
        AnyMap doc = AnyMap::fromYamlString("species: " + fuzzcommon::str(data, size));
        auto& snode = doc["species"];
        vector<AnyMap> nodes;
        if (snode.isVector<AnyMap>()) {
            nodes = snode.asVector<AnyMap>();
        } else if (snode.is<AnyMap>()) {
            nodes.push_back(snode.as<AnyMap>());
        }
        IdealGasPhase gas;
        gas.addUndefinedElements();
        for (auto& node : nodes) {
            node.applyUnits();
            try {
                shared_ptr<Species> sp = newSpecies(node);
                if (sp->thermo) {
                    double cp, h, s;
                    for (double T : {200.0, 298.15, 1000.0, 3500.0}) {
                        sp->thermo->updatePropertiesTemp(T, cp, h, s);
                    }
                    (void)sp->thermo->minTemp();
                    (void)sp->thermo->maxTemp();
                }
                (void)sp->parameters(nullptr, true);
                gas.addSpecies(sp);
            } catch (const std::exception&) {
            }
        }
        if (gas.nSpecies()) {
            gas.initThermo();
            gas.setState_TP(1200.0, 101325.0);
            vector<double> mu(gas.nSpecies());
            gas.getChemPotentials(mu);
            (void)gas.enthalpy_mass();
            (void)gas.report(true);
        }
    } catch (const std::exception&) {
    }
    return 0;
}
