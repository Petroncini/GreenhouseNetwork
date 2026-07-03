CXX = g++
CXXFLAGS = -std=c++17 -pthread

TARGETS = manager sensor actuator controller

all: $(TARGETS)

manager: manager.cpp protocol.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

sensor: sensor.cpp protocol.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

actuator: actuator.cpp protocol.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

controller: controller.cpp protocol.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f $(TARGETS)

.PHONY: all clean
