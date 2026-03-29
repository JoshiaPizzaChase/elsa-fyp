# Market Simulation for Algorithmic Trading

C++ Version: C++23

FIX Version: FIX4.2

# Setup

Note: Please make sure the boost version >= 1.86.0

## Dependencies Installation
```
./scripts/setup.sh
```

## libpqxx installation
1. First ensure you installed any postgresql related libraries on your system, perhaps via the package manager.
This includes `libpq`, the underlying C-library powering `libpqxx`.
2. It is recommended to install libpqxx from source, and important to disable shared libraries. Without it, you *may* encounter double-free issues.
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
mkdir deps
cd deps
git clone https://github.com/questdb/c-questdb-client.git
cd ..
```
