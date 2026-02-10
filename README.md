# gtfs2rdf
a command line C++-tool for efficient conversion of transit data from GTFS to RDF

## Run
Make sure to have a C++ compiler installed that supports C++-20. You might need to specify that compiler to cmake if there are older compilers on your system as well. Furthermore, ensure Cmake version 3.28 or newer is used.

**Warning:** g++-14 throws internal compiler errors on my system (likely due to things related to modules). clang++-18 works fine, but g++-15 might also do the trick.

### Release build

```bash
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++-18
cmake --build build-release -j
```

### Profiling  build

```bash
cmake -S . -B build-prof -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++-18
cmake --build build-prof -j
```

### Debub build

```bash
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++-18
cmake --build build-debug -j

```

### Release but with all stats enabled

```bash
cmake -S . -B build-release-stats -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++-18 \
  -DCMAKE_BUILD_TYPE=Release \
  -DGTFS2RDF_FULL_STATS=ON
cmake --build build-release-stats -j
```

*Note:* Add build option ```-DCMAKE_EXPORT_COMPILE_COMMANDS=ON``` in order to use clang-tidy afterwards.

### Build with tests

```bash
cmake -S . -B build-tests -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON -DCMAKE_CXX_COMPILER=clang++-18
cmake --build build-tests -j
ctest --test-dir build-tests --output-on-failure
# unit tests only:
ctest --test-dir build-tests -L unit --output-on-failure
# end-to-end tests only:
ctest --test-dir build-tests -L e2e --output-on-failure
```

### Build for analysing test coverage

```bash
cmake -S . -B build-coverage -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DCMAKE_CXX_COMPILER=clang++-18 \
  -DCMAKE_C_COMPILER=clang-18 \
  -DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping" \
  -DCMAKE_C_FLAGS="-fprofile-instr-generate -fcoverage-mapping" \
  -DCMAKE_EXE_LINKER_FLAGS="-fprofile-instr-generate" \
  -DCMAKE_SHARED_LINKER_FLAGS="-fprofile-instr-generate"

cmake --build build-coverage -j

rm -rf build-coverage/tests/profiles
mkdir -p build-coverage/tests/profiles

# (Option A) exactly your behavior (relative "profiles/" ends up under build-coverage/tests/)
LLVM_PROFILE_FILE="profiles/%p.profraw" \
  ctest --test-dir build-coverage --output-on-failure

# optional later
# LLVM_PROFILE_FILE="build-coverage/tests/profiles/cli_%p.profraw" \
#   build-coverage/gtfs2rdf /path/to/feed.zip

llvm-profdata merge -sparse build-coverage/tests/profiles/*.profraw \
  -o build-coverage/tests/coverage.profdata

llvm-cov show build-coverage/gtfs2rdf \
  -object=build-coverage/tests/unit_tests \
  -object=build-coverage/tests/e2e_tests \
  -instr-profile=build-coverage/tests/coverage.profdata \
  -format=html -output-dir=build-coverage/tests/coverage-html \
  -ignore-filename-regex='(^|/)(third_party|_deps|build-coverage|tests)(/|$)'
```



## Style checking

Run this command to enforce style and naming conventions for this project using clang-tidy.

```bash
find src tests \
  -path 'src/third_party' -prune -o \
  -type f \( \
    -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' -o \
    -name '*.h' -o -name '*.hh' -o -name '*.hpp' -o -name '*.hxx' -o \
    -name '*.ixx' -o -name '*.cppm' \
  \) -print0 \
| xargs -0 -n 1 -P 1 \
    clang-tidy-18 -p build-release \
      --use-color=false \
      --system-headers=false \
      --header-filter='(^|.*/)(src|tests)/(?!third_party/).*' \
  2>&1 | tee clang-tidy.log
```

## Testing

To create custom mock data (in a zip archive since this is the expected input) use the command below
from root. Instead of `stops.txt`, you can specify any files from that directory (!) to make it into
the resulting zip file

```bash
(cd mock_data && zip romania_mock.zip stops.txt)
```

### wkt Linestring ordering correctly retained

To check, that a LINESTRING was computed in correct order
note that this compares the LINESTRING corresponding to `shape_id = 1`

```bash
diff -u \
  <(awk -F',' '
     NR==1{for(i=1;i<=NF;i++){if($i=="shape_id")sid=i;if($i=="shape_pt_lon")lon=i;if($i=="shape_pt_lat")lat=i;if($i=="shape_pt_sequence")seq=i} next}
     $sid=="1"{print $seq "\t" $lon "\t" $lat}
   ' '../data/öv_de_shapes/shapes.txt' \
   | sort -n -k1,1 \
   | awk 'BEGIN{printf "LINESTRING("}{if(NR>1)printf ", "; printf "%s %s",$2,$3}END{print ")"}') \
  <(awk '
     $1=="gtfs2rdfgeom:shapes_1" && $2=="geo:asWKT" {
       if (match($0, /"LINESTRING\([^"]*\)"/)) { print substr($0, RSTART+1, RLENGTH-2); exit }
     }
   ' 'öv_de_shapes.ttl')
```



## Benchmarking

For benchmarking, make sure to clear cashes to ensure comparability, using

```bash
sudo free && sync && sudo sh -c 'echo 3 >/proc/sys/vm/drop_caches' && free
```

Plot RAM-Usage using (example dataset, modify as desired)
```bash
psrecord "build-release/gtfs2rdf ../data/öv_de_shapes.zip --stats verbose" --interval 0.5 --include-children --plot ram.png --log ram.log
```

## Third-party code
This repository vendors third-party components under `src/third_party/`.
See `THIRD_PARTY_NOTICES.md` for details and license information.
