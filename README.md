# gtfs2rdf
a command line C++-tool for efficient conversion of transit data from GTFS to RDF

## Run
Make sure to have a C++ compiler installed that supports C++-20. You might need to specify that compiler to cmake if there are older compilers on your system as well.

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++-14
cmake --build build -j
```
