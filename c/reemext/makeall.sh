#!/bin/bash

g++ reeme.cpp -shared -o ../../framework/reemext.so -I./luajit -L./luajit -lluajit
