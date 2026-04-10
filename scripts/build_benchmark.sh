#!/bin/bash

cmake -B build-benchmark \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_BENCHMARKS=ON \
    -DBUILD_TESTS=OFF \
    -DCMAKE_C_FLAGS="-DNDEBUG" \
    -DCMAKE_CXX_FLAGS="-DNDEBUG -DBOOST_DISABLE_ASSERTS -DBOOST_CONTRACT_NO_PRECONDITIONS -DBOOST_CONTRACT_NO_POSTCONDITIONS -DBOOST_CONTRACT_NO_EXCEPTS -DBOOST_CONTRACT_NO_INVARIANTS"
cmake --build build-benchmark --target mpsc_ring_buffer_benchmark matching_engine_benchmark
