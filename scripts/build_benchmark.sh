#!/bin/bash

cmake -B build-benchmark -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON -DBUILD_TESTS=OFF
cmake --build build-benchmark
