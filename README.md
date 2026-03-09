# Market Simulation for Algorithmic Trading

C++ Version: C++23

FIX Version: FIX4.2

# Setup

## reflectcpp Installation

```bash
git clone https://github.com/getml/reflect-cpp.git
cd reflect-cpp
cmake -S . -B build -DCMAKE_CXX_STANDARD=20  -DREFLECTCPP_TOML=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo cmake --install build
```

## toml++ Installation

```bash
git clone https://github.com/marzer/tomlplusplus.git
cd tomlplusplus/
mkdir build
cd build
cmake ..
sudo make install
```

## SPDLOG

For any new target that uses spdlog, please include a compiler flag:
```
target_compile_definitions(<target_name> <visibility> SPDLOG_USE_STD_FORMAT)
```

## libpqxx installation
It is recommended to install libpqxx from source, and important to disable shared libraries. Without it, you *may* encounter double-free issues.
```bash
git clone https://github.com/jtv/libpqxx.git
cd libpqxx
./configure --disable-shared
make
sudo make install
```

## questdb installation
Run this and you should see `deps/c-questdb-client/...` in your project directory:
```bash
git subtree add --prefix deps/c-questdb-client https://github.com/questdb/c-questdb-client.git 6.0.0 --squash
```
