CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
INC      := -Iinclude -Ibench
BUILD    := build
HEADERS  := $(wildcard include/dsu/*.hpp) bench/workloads.hpp

all: $(BUILD)/dsu_bench $(BUILD)/test_dsu $(BUILD)/componentes $(BUILD)/kruskal

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/dsu_bench: bench/bench.cpp $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(INC) $< -o $@

$(BUILD)/test_dsu: tests/test_dsu.cpp $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(INC) $< -o $@

$(BUILD)/componentes: examples/componentes.cpp $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(INC) $< -o $@

$(BUILD)/kruskal: examples/kruskal.cpp $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(INC) $< -o $@

test: $(BUILD)/test_dsu
	./$(BUILD)/test_dsu

# Testes também com verificação de memória e comportamento indefinido.
test-sanitize: | $(BUILD)
	$(CXX) -std=c++17 -O1 -g -fsanitize=address,undefined $(INC) \
		tests/test_dsu.cpp -o $(BUILD)/test_dsu_san
	./$(BUILD)/test_dsu_san

clean:
	rm -rf $(BUILD)

.PHONY: all test test-sanitize clean
