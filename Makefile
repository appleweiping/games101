# Build every GAMES101 assignment with g++ (C++17, -O2, OpenMP).
# Usage:  mingw32-make        (build all)
#         mingw32-make a7     (build one)
#         mingw32-make run    (build + run all -> results/)
# Fetch deps first:  bash scripts/setup_deps.sh   (or scripts/setup_deps.ps1)

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -fopenmp -Ithird_party/eigen -Ithird_party -Icommon
STB      := common/stb_impl.cpp
BUILD    := build

IDS  := a0 a1 a2 a3 a4 a5 a6 a7
a0_DIR := a0_transform
a1_DIR := a1_triangle
a2_DIR := a2_rasterizer
a3_DIR := a3_shading
a4_DIR := a4_bezier
a5_DIR := a5_whitted
a6_DIR := a6_bvh
a7_DIR := a7_pathtracing

.PHONY: all run clean $(IDS)
all: $(IDS)

define RULE
$(1): $(BUILD)/$(1).exe
$(BUILD)/$(1).exe: assignments/$($(1)_DIR)/main.cpp $(STB) | $(BUILD)
	$(CXX) $(CXXFLAGS) $$< $(STB) -o $$@
endef
$(foreach id,$(IDS),$(eval $(call RULE,$(id))))

$(BUILD):
	mkdir -p $(BUILD)

run: all
	@for id in $(IDS); do echo "== $$id =="; ./$(BUILD)/$$id.exe; done

clean:
	rm -rf $(BUILD)
