mkdir deps
cd deps
# install Catch2
git clone https://github.com/catchorg/Catch2.git
cd Catch2
cmake -B build -S . -DBUILD_TESTING=OFF
sudo cmake --build build/ --target install
cd ..
# install QuickFIX
git clone https://github.com/quickfix/quickfix.git
cd quickfix
./bootstrap
./configure
cmake -S . -B build
cmake --build build -j 4
sudo cmake --install build
cd ..
# install spdlog
git clone https://github.com/gabime/spdlog.git
cd spdlog && mkdir build && cd build
cmake .. && cmake --build .
sudo make install
cd ../..
# install toml++
git clone https://github.com/marzer/tomlplusplus.git
cd tomlplusplus/
mkdir build
cd build
cmake ..
sudo make install
cd ..
# install reflectcpp
git clone https://github.com/getml/reflect-cpp.git
cd reflect-cpp
git submodule update --init
./vcpkg/bootstrap-vcpkg.sh
cmake -S . -B build -DCMAKE_CXX_STANDARD=20 -DREFLECTCPP_TOML=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
sudo cmake --install build
cd ..
# install protobuf
git clone https://github.com/protocolbuffers/protobuf.git
cd protobuf
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 10
sudo cmake --install build
cd ..
# install nlohmann json
git clone https://github.com/nlohmann/json.git
cd json
cmake -S . -B build
cmake --build build -j 4
sudo cmake --install build
cd ..

echo "Note: There are still dependencies to be installed. Installation methods may vary based on your OS,"
echo "like websocketpp, boost, libpqxx, etc."
echo "Please check README.md"
