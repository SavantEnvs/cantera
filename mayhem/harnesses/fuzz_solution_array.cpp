// SolutionArray deserializer + index API. Input layout:
//   bytes 0-1: int16 loc_a, bytes 2-3: int16 loc_b, byte 4: initial array size,
//   byte 5: slice selector; the rest is a YAML SolutionArray file (as written by
//   SolutionArray::save). The array is restored with readEntry() under the file's
//   first top-level key, then fuzz-chosen (possibly negative / out-of-range)
//   locations go through setLoc/getState/setState/getAuxiliary on the array and a
//   share()d slice of it (the checks fixed by upstream f404dd20f6).
#include "fuzz_common.h"
#include "cantera/base/AnyMap.h"
#include "cantera/base/Solution.h"
#include "cantera/base/SolutionArray.h"
#include "cantera/thermo/ThermoPhase.h"
#include <cstring>
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

static void poke(SolutionArray& arr, int loc) {
    try { arr.setLoc(loc); } catch (const std::exception&) {}
    try {
        auto st = arr.getState(loc);
        arr.setState(loc, st);
    } catch (const std::exception&) {}
    try { (void)arr.getAuxiliary(loc); } catch (const std::exception&) {}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 6) {
        return -1;
    }
    int16_t la, lb;
    std::memcpy(&la, data, 2);
    std::memcpy(&lb, data + 2, 2);
    int n = data[4] % 32;
    uint8_t sel = data[5];
    std::string yaml = fuzzcommon::str(data + 6, size - 6);
    // the shared Solution's state is per-input: reset it
    g_sol->thermo()->restoreState(g_state);
    try {
        auto arr = SolutionArray::create(g_sol, n);
        try {
            AnyMap root = AnyMap::fromYamlString(yaml);
            auto keys = root.keys();
            if (!keys.empty()) {
                const string& name = *keys.begin();
                try { (void)SolutionArray::readHeader(root, name); } catch (const std::exception&) {}
                arr->readEntry(root, name, "");
            }
        } catch (const std::exception&) {
        }
        poke(*arr, la);
        poke(*arr, lb);
        try {
            vector<int> idx;
            for (int i = 0; i < 8; i++) {
                if (sel & (1 << i)) {
                    idx.push_back(i % 2 ? (int)lb + i : (int)la - i);
                }
            }
            auto slice = arr->share(idx);
            poke(*slice, la);
            poke(*slice, lb);
            try { (void)slice->info({}, 5, 80); } catch (const std::exception&) {}
        } catch (const std::exception&) {}
        try { (void)arr->info({}, 5, 80); } catch (const std::exception&) {}
        // resize() takes an element count; a negative one breaks its precondition
        // (it would become SIZE_MAX), so keep the fuzz-chosen size small and >= 0
        try { arr->resize(la & 0x1f); } catch (const std::exception&) {}
        poke(*arr, lb);
    } catch (const std::exception&) {
    }
    return 0;
}
