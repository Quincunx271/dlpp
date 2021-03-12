CXXFLAGS += -fPIC

all: libcounter.so

libcounter.so:
	$(CXX) $(CXXFLAGS) -shared counter.cpp -o libcounter.so
