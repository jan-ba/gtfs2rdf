# syntax=docker/dockerfile:1
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# 
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates git \
    clang-18 clang-tools-18 lld-18 \
    llvm-18 \
    cmake ninja-build \
    pkg-config \
    libzip-dev zipmerge zipcmp ziptool \
    zip \
    python3 \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/gtfs2rdf

# copy repo into image
COPY . .

# build release binary (fastest)
RUN cmake -S . -B build-release -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=clang++-18 \
      -DCMAKE_C_COMPILER=clang-18 \
      -DCMAKE_CXX_COMPILER_CLANG_SCAN_DEPS=/usr/bin/clang-scan-deps-18 \
  && cmake --build build-release -j

# build tests (for verification)
RUN cmake -S . -B build-tests -G Ninja \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DBUILD_TESTING=ON \
      -DCMAKE_CXX_COMPILER=clang++-18 \
      -DCMAKE_C_COMPILER=clang-18 \
      -DCMAKE_CXX_COMPILER_CLANG_SCAN_DEPS=/usr/bin/clang-scan-deps-18 \
  && cmake --build build-tests -j


# -----------------------------------------------------------------------------
# repro commands (Docker):
# docker build -t jan-babin-thesis .
# docker run -it --rm \
#   -v /local/data/<username>:/extern/local \
#   jan-babin-thesis \
#   /extern/local/<your-feed>.zip \
#   --output /extern/local/out/<runname>
#
# repro commands (wharfer):
# wharfer build -t jan-babin-thesis .
# wharfer run -it --rm \
#   -v /local/data/<username>:/extern/local \
#   jan-babin-thesis /extern/local/<your-feed>.zip  \
#   --output /extern/local/out/<runname>
# -----------------------------------------------------------------------------
