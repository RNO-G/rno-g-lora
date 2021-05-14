

all: 3rdparty build/rno-g-lora-web build/rno-g-lora-bridge
3rdparty:  3rdparty/include/boost 3rdparty/include/crow_all.h

CXXFLAGS+=-Os -g -I3rdparty/include -Wall -Wextra -I/rno-g/include
LDFLAGS+=-lpthread -L3rdparty/lib -lboost_system -lboost_container -lboost_json -lz -lmosquitto -lpq

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

3rdparty/boost_1_76_0.tar.gz: 
	mkdir -p 3rdparty
	cd 3rdparty && wget https://pilotfiber.dl.sourceforge.net/project/boost/boost/1.76.0/boost_1_76_0.tar.gz
	


clean: 
	rm -rf build
