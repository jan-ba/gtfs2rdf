# gtfs2rdf
a command line C++-tool for efficient conversion of transit data from GTFS to RDF

## Run
Make sure to have a C++ compiler installed that supports C++-20. You might need to specify that compiler to cmake if there are older compilers on your system as well. Furthermore, ensure Cmake version 3.28 or newer is used.

**Warning:** g++-14 throws internal compiler errors on my system (likely due to things related to modules). clang++-18 works fine, but g++-15 might also do the trick.

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++-18
cmake --build build -j
```

## Testing

To create custom mock data (in a zip archive since this is the expected input) use the command below
from root. Instead of `stops.txt`, you can specify any files from that directory (!) to make it into
the resulting zip file

```bash
(cd mock_data && zip romania_mock.zip stops.txt)
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