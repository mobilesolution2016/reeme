#!/bin/bash

clang reeme.cpp crtopt.cpp -undefined dynamic_lookup -shared -I./luajit -L./luajit -lluajit -lz -O2 -fvisibility=hidden -o ../../framework/libreemext.so
