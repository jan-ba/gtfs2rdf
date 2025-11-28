# gtfs2rdf
a command line C++-tool for efficient conversion of transit data from GTFS to RDF

## Run
Make sure to have a C++ compiler installed that supports C++-20. You might need to specify that compiler to cmake if there are older compilers on your system as well.

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++-14
cmake --build build -j
```

## Testing

To create custom mock data (in a zip archive since this is the expected input) use the command below
from root. Instead of `stops.txt`, you can specify any files from that directory (!) to make up the final zip
```bash
(cd mock_data && zip romania_mock.zip stops.txt)
```

## Benchmarking

For benchmarking, make sure to clear cashes to ensure comparability, using

```bash
sudo free && sync && sudo sh -c 'echo 3 >/proc/sys/vm/drop_caches' && free
```