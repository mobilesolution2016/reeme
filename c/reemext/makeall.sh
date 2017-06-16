#osx
#gcc reeme.cpp crtopt.cpp -undefined dynamic_lookup -shared -I./qrcode -I./luajit -L./luajit -lluajit -lz -O2 -fvisibility=hidden -o ../../framework/libreemext.so
#linux
#g++ reeme.cpp crtopt.cpp -shared -I./qrcode -I./luajit -L./luajit -lluajit -lz -O2 -fvisibility=hidden -std=c++11 -fPIC -o ../../framework/libreemext.so
