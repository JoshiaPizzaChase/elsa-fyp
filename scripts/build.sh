#!/bin/bash

cmake -B build 
cmake --build build

sudo mkdir -p /usr/local/bin/deployment-server
sudo cp -r build /usr/local/bin/deployment-server/