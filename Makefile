.PHONY: clean run
syxlog: syxlog.cpp
	$(CXX) $^ -std=c++14 -framework CoreMIDI -framework CoreFoundation -o $@
run: syxlog
	./syxlog
clean:
	rm syxlog
