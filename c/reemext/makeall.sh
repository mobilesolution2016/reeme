#!/bin/bash

clang reeme.cpp crtopt.cpp -undefined dynamic_lookup -shared -I./luajit -L./luajit -lluajit -O2 -o ../../framework/reemext.so
