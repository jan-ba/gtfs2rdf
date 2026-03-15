FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    bash \
    clang-18 \
    clang-tools-18 \
    llvm-18 \
    cmake \
    ninja-build \
    make \
    libzip-dev \
    zipcmp \
    zipmerge \
    ziptool \
    ca-certificates \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/gtfs2rdf
COPY . /opt/gtfs2rdf

ENV CC=clang-18
ENV CXX=clang++-18

RUN cmake -S . -B build-release -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=${CC} \
    -DCMAKE_CXX_COMPILER=${CXX} \
 && cmake --build build-release -j"$(nproc)"

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    bash \
    ca-certificates \
    libstdc++6 \
    libzip4t64 \
    zipcmp \
    zipmerge \
    ziptool \
    zip \
    unzip \
 && rm -rf /var/lib/apt/lists/*

COPY --from=build /opt/gtfs2rdf/build-release/gtfs2rdf /usr/local/bin/gtfs2rdf

WORKDIR /work
ENTRYPOINT ["/usr/local/bin/gtfs2rdf"]
CMD ["--help"]
