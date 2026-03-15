# gtfs2rdf

A command-line C++ tool for converting public transport data from **GTFS** to **RDF**.

`gtfs2rdf` is designed for fast, practical conversion of real-world transit feeds. It focuses on efficient processing, bounded memory usage, and flexible mappings so that different GTFS feeds can be turned into RDF in a way that fits the needs of downstream applications such as search, analysis, and visualisation.

## What it does

Given a GTFS feed as a `.zip` archive, `gtfs2rdf` reads the feed and writes RDF output in either **Turtle** (`ttl`) or **N-Triples** (`nt`) format.

The tool is built around the idea that GTFS feeds are not always uniform in practice. For that reason, mappings are customisable: users can define how GTFS fields should be turned into RDF by providing schema modules for the relevant GTFS files.

In short, the project aims to provide:

* **Fast conversion**
* **Bounded memory usage**, even for more complex mappings
* **Support for data transformations** and cross-file lookups
* **Customisable mappings** for different feeds and modelling needs

## Current state

At the moment, mappings are part of the build.  
That means that if you want to add or change a mapping, you currently need to:

1. Create or modify a schema module
2. Place it in `src/schema/`
3. Run the full build pipeline again

This is a deliberate but temporary design choice. A future improvement is to make mappings selectable at runtime through a CLI option so that users can point the converter to a mapping directory without rebuilding.

## Requirements

### Build requirements

`gtfs2rdf` uses **C++20**, including **C++20 modules**. This is relevant both for the compiler and for parts of the implementation that rely on modern standard library features such as `chrono`.

The project currently requires:

* A compiler with solid **C++20 module** support
* **CMake 3.28 or newer**
* **Ninja**
* **libzip**
* The required Clang dependency scanning tools

In practice:

* **clang-18** is the recommended and tested compiler
* **g++-15** may work, but was not the primary tested setup
* **g++-14** caused internal compiler errors during development

A working setup therefore typically includes:

* `clang-18`
* `clang-tools-18`
* `llvm-18`
* `cmake`
* `ninja-build`
* `libzip-dev`
* `zipmerge`
* `zipcmp`
* `ziptool`

## Getting started

### Release build

```bash
cmake -S . -B build-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang-18 \
  -DCMAKE_CXX_COMPILER=clang++-18

cmake --build build-release -j
```

After that, run:

```bash
build-release/gtfs2rdf --help
```

### Basic usage

```bash
gtfs2rdf [options] GTFS_ZIP
```

**Examples:**

```bash
gtfs2rdf feed.zip --format nt
gtfs2rdf --feed feed.zip --output out/
```

When run without arguments beyond the input file, the tool writes output to the current directory in Turtle format.

## Common options

### Standard options

* **-o, --output arg** Output directory or stdout (Default: `.`)
* **-p, --pre-run** Validate schemas, print file headers, and estimate output size and peak RAM usage without writing output
* **--format arg** Output format: `ttl` or `nt` (Default: `ttl`)
* **--overwrite** Overwrite existing output files
* **-h, --help** Show help

### RAM / runtime options

* **--read-buffer-size arg** Read buffer size in MB
* **--write-buffer-size arg** Write buffer size in MB
* **--storage-buffer-size arg** RAM budget in MB for persistent storage cache used between GTFS files
* **--tmp-dir arg** Temporary directory for intermediate files

### Diagnostics / advanced options

* **--spec-dump** Dump the mapping specification to disk
* **--warning-level arg** Warning verbosity: `quiet`, `warning`, `debug`
* **--stats arg** Statistics output: `quiet`, `brief`, `verbose`

## Recommended workflow

For unfamiliar or large feeds, a pre-run is recommended before actual conversion:

```bash
build-release/gtfs2rdf --pre-run --stats verbose --spec-dump feed.zip
```

This allows you to:

* Validate the input against the expected schemas
* Inspect conversion details
* Estimate output size
* Get an early idea of memory requirements

Then run the actual conversion once everything looks right.

## Customising mappings

Mappings are implemented as schema modules in:

`src/schema/`

To add a custom mapping, create a new schema module or adapt an existing one and place it in that directory. A template is provided in:

`src/schema/schema_template.cppm`

This template serves as both a starting point and lightweight documentation of the embedded mapping DSL. At the moment, mapping changes require a rebuild of the project, including schema discovery.

## Docker

A simple Docker image can be built for running the converter on your own files.

**Example:**

```bash
docker build -t gtfs2rdf .
```

Then run it with a mounted input directory, for example:

```bash
docker run --rm \
  -v "$(realpath ../data):/data:ro" \
  gtfs2rdf /data/feed.zip
```

Note that files on the host are only visible inside the container if they are mounted explicitly.

## Testing and measurement

For regular use, a normal release build is enough. For development, testing, or timing measurements, the following builds 
can be useful.

### Release build with full statistics

```bash
cmake -S . -B build-release-stats -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DGTFS2RDF_FULL_STATS=ON \
  -DGTFS2RDF_ENABLE_IPO=OFF \
  -DCMAKE_C_COMPILER=clang-18 \
  -DCMAKE_CXX_COMPILER=clang++-18

cmake --build build-release-stats -j
```

### Build with tests

```bash
cmake -S . -B build-tests -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DGTFS2RDF_ENABLE_IPO=OFF \
  -DCMAKE_C_COMPILER=clang-18 \
  -DCMAKE_CXX_COMPILER=clang++-18 

cmake --build build-tests -j
```

## Third-party code

This repository vendors third-party components under `src/third_party/`. Please see:

`THIRD_PARTY_NOTICES.md`

for details and license information.

## If you use or discuss this project

If this repository is used in other work, discussed elsewhere, or built upon in a public project, a link back to this repository would be very welcome.
