git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
git clone https://github.com/getml/reflect-cpp.git
cd reflect-cpp
vcpkg install reflectcpp
