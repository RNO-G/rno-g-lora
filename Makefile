

all: 3rdparty build/rno-g-lora-web build/rno-g-lora-bridge
3rdparty:  3rdparty/include/boost 3rdparty/include/crow_all.h 3rdparty/include/CLI11.hpp

CXXFLAGS+=-Os -g -I3rdparty/include -Wall -Wextra -I/rno-g/include
LDFLAGS+=-lpthread -L3rdparty/lib -lboost_system -lboost_container -lboost_json -lz -lmosquitto -lpq -Wl,-rpath='$$ORIGIN/../3rdparty/lib' 

RNO_G_INSTALL_DIR?=/rno-g/
PREFIX?=$(RNO_G_INSTALL_DIR)



install: 
	mkdir -p $(PREFIX)/3rdparty/lib
	mkdir -p $(PREFIX)/bin
	# have to install appropriate  boost_sytem/boost_container/ boost_json, but we'll put them elsewhere in the tree and make sure our RPATH is correct! 
	install 3rdparty/lib/libboost_system.so* $(PREFIX)/3rdparty/lib/
	install 3rdparty/lib/libboost_container.so* $(PREFIX)/3rdparty/lib/
	install 3rdparty/lib/libboost_json.so* $(PREFIX)/3rdparty/lib/
	install build/rno-g-lora-bridge $(PREFIX)/bin
	install build/rno-g-lora-web $(PREFIX)/bin
	


build: 
	mkdir -p $@

build/%: src/%.cc src/rno-g-lora-common.h | 3rdparty build
	c++ $(CXXFLAGS)  -o $@ $^ $(LDFLAGS) 


3rdparty/include/boost: 3rdparty/boost_1_76_0.tar.gz 
	mkdir -p $@
	rm -rf boost_1_76_0
	cd 3rdparty && tar xfv boost_1_76_0.tar.gz && cd boost_1_76_0 && ./bootstrap.sh --prefix=../ && ./b2 install --prefix=../

3rdparty/include/crow_all.h:
	mkdir -p 3rdparty/include
	cd 3rdparty/include && wget https://github.com/CrowCpp/crow/releases/download/0.2/crow_all.h

3rdparty/include/CLI11.hpp:
	mkdir -p 3rdparty/include
	cd 3rdparty/include && wget https://github.com/CLIUtils/CLI11/releases/download/v1.9.1/CLI11.hpp


3rdparty/boost_1_76_0.tar.gz: 
	mkdir -p 3rdparty
	cd 3rdparty && wget https://pilotfiber.dl.sourceforge.net/project/boost/boost/1.76.0/boost_1_76_0.tar.gz
	


clean: 
	rm -rf build
