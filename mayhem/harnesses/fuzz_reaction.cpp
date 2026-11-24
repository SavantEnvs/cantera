// Reaction / rate parsers: the input is a YAML list of reaction entries, each
// built with newReaction(node, kinetics) against an H2/O2 gas (species from
// h2o2.yaml, no reactions), added to that kinetics object, then rate constants
// are evaluated and each reaction is serialized back with parameters().
#include "fuzz_common.h"
#include "cantera/base/AnyMap.h"
#include "cantera/base/Solution.h"
#include "cantera/thermo/ThermoPhase.h"
#include "cantera/kinetics/Kinetics.h"
#include "cantera/kinetics/Reaction.h"
#include <vector>

using namespace Cantera;

static AnyMap g_phase;
static AnyMap g_root;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
    fuzzcommon::init();
    g_root = AnyMap::fromYamlString(
        "phases:\n"
        "- name: gas\n"
        "  thermo: ideal-gas\n"
        "  elements: [O, H, Ar, N, E]\n"
        "  species: [{h2o2.yaml/species: all}]\n"
        "  kinetics: gas\n"
        "  reactions: none\n"
        "  state: {T: 1000.0, P: 1 atm, X: {H2: 2.0, O2: 1.0, AR: 1.0, H: 0.1, OH: 0.1}}\n");
    g_phase = g_root["phases"].asVector<AnyMap>()[0];
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // fresh phase + kinetics per input so added reactions never carry over
    shared_ptr<Solution> sol;
    try {
        sol = newSolution(g_phase, g_root, "none");
    } catch (const std::exception&) {
        return 0;
    }
    auto kin = sol->kinetics();
    try {
        AnyMap doc = AnyMap::fromYamlString("reactions: " + fuzzcommon::str(data, size));
        auto& rnode = doc["reactions"];
        vector<AnyMap> rxns;
        if (rnode.isVector<AnyMap>()) {
            rxns = rnode.asVector<AnyMap>();
        } else if (rnode.is<AnyMap>()) {
            rxns.push_back(rnode.as<AnyMap>());
        }
        for (auto& node : rxns) {
            node.applyUnits();
            try {
                shared_ptr<Reaction> r = newReaction(node, *kin);
                (void)r->equation();
                (void)r->parameters(true);
                kin->addReaction(r);
            } catch (const std::exception&) {
            }
        }
        if (kin->nReactions()) {
            vector<double> kf(kin->nReactions()), rop(kin->nReactions());
            vector<double> wdot(kin->nTotalSpecies());
            kin->getFwdRateConstants(kf);
            kin->getFwdRatesOfProgress(rop);
            kin->getNetProductionRates(wdot);
            for (size_t i = 0; i < kin->nReactions(); i++) {
                (void)kin->reaction(i)->parameters(true);
            }
        }
    } catch (const std::exception&) {
    }
    return 0;
}
