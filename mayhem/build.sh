#!/usr/bin/env bash
#
# mayhem/build.sh — build Cantera's fuzz harnesses and its upstream googletest suites.
#
#  1) Upstream SCons build with NORMAL flags (libcantera + the thermo/kinetics/transport/equil
#     gtest programs that mayhem/test.sh runs). Submodule deps come from Debian -dev packages
#     (system_*=y), so the build is offline. It also generates include/cantera/base/config.h.
#  2) The library sources (src/{base,thermo,tpx,equil,numerics,kinetics,transport,oneD,zeroD})
#     compiled AGAIN with $SANITIZER_FLAGS $DEBUG_FLAGS into mayhem/out/obj (incremental make,
#     gitignored via mayhem/.gitignore), archived as mayhem/out/libcantera_fuzz.a.
#  3) Each harness in mayhem/harnesses/ linked twice: libFuzzer (/mayhem/<name>) and the
#     run-once standalone reproducer (/mayhem/<name>-standalone).
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

# SUNDIALS built from upstream's pinned submodule commit by the Dockerfile (no MPI).
SUNDIALS=/opt/toolchains/deps/sundials-7.5

# 1) upstream build + test programs, normal flags
scons build build-thermo build-kinetics build-transport build-equil -j"$MAYHEM_JOBS" \
  CC="$CC" CXX="$CXX" \
  python_package=n f90_interface=n hdf_support=n use_pch=n \
  googletest=system system_eigen=y system_fmt=y system_yamlcpp=y system_sundials=y \
  sundials_include="$SUNDIALS/include" sundials_libdir="$SUNDIALS/lib" debug=n optimize=y

# 2) sanitized library
OUT="$SRC/mayhem/out"
mkdir -p "$OUT/obj"
FUZZ_CXXFLAGS="-std=c++20 $SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link"
INCS="-I$SRC/include -I$SRC/src -isystem /usr/include/eigen3 -isystem $SUNDIALS/include"
cat > "$OUT/Makefile" <<EOF
SRCS := \$(wildcard \$(addprefix $SRC/src/,\$(addsuffix /*.cpp,base thermo tpx equil numerics kinetics transport oneD zeroD))) $SRC/src/extensions/canteraStatic.cpp
OBJS := \$(patsubst $SRC/src/%.cpp,$OUT/obj/%.o,\$(SRCS))
CXXFLAGS := $FUZZ_CXXFLAGS $INCS -DCANTERA_DATA=\\"/usr/local/share/cantera/data\\" -DGIT_COMMIT=\\"mayhem\\"
$OUT/libcantera_fuzz.a: \$(OBJS)
	rm -f \$@ && ar rcs \$@ \$^
$OUT/obj/%.o: $SRC/src/%.cpp $SRC/include/cantera/base/config.h
	@mkdir -p \$(dir \$@)
	$CXX \$(CXXFLAGS) -MMD -MP -c \$< -o \$@
-include \$(OBJS:.o=.d)
EOF
make -f "$OUT/Makefile" -j"$MAYHEM_JOBS"

# 3) harnesses
LIBS="$OUT/libcantera_fuzz.a -L$SUNDIALS/lib -lsundials_cvodes -lsundials_idas -lsundials_sunlinsoldense -lsundials_sunlinsolband -lsundials_sunmatrixdense -lsundials_sunmatrixband -lsundials_nvecserial -lsundials_core -lyaml-cpp -lfmt -lpthread -lm"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/standalone_main.o"
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$OUT/lsan_off.o"
for h in "$SRC"/mayhem/harnesses/*.cpp; do
  name=$(basename "$h" .cpp)
  $CXX $FUZZ_CXXFLAGS $INCS -c "$h" -o "$OUT/$name.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$OUT/$name.o" "$OUT/lsan_off.o" $LIBS -o "/mayhem/$name"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$OUT/standalone_main.o" "$OUT/$name.o" "$OUT/lsan_off.o" $LIBS -o "/mayhem/$name-standalone"
done
