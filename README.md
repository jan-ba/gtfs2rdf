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

## Style checking

Run this command to enforce style and naming conventions for this project using clang-tidy.

```bash
find src -path 'src/third_party' -prune -o -type f \( \
    -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' -o \
    -name '*.h' -o -name '*.hh' -o -name '*.hpp' -o -name '*.hxx' -o \
    -name '*.ixx' -o -name '*.cppm' \
  \) -print0 \
| xargs -0 -n 1 -P 1 \
    clang-tidy-18 -p build-release \
      --use-color=false \
      --system-headers=false \
      --header-filter='(^|.*/)src/(?!third_party/).*' \
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
psrecord "build/gtfs2rdf ../data/öv_de_shapes.zip" --interval 0.5 --include-children --plot ram.png --log ram.log
```

## Third-party code
This repository vendors third-party components under `src/third_party/`.
See `THIRD_PARTY_NOTICES.md` for details and license information.
