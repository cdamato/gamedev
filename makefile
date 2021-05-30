SOURCE_DIRS := src/common/ src/ecs/ src/engine/ src/display/ src/world/ src/ui/
TEST_DIRS := tests/

CPP_DEFINES :=
LINUX_LIBS := Xext freetype X11 GL
AMD64_FLAGS := -Darch_amd64
DEBUG_FLAGS := -Wno-unused-parameter -fsanitize=undefined -fsanitize=address -g3 -Wall -Wextra

ifneq ($(OPENGL),false)
	LINUX_LIBS := $(LINUX_LIBS) GLEW
	CPP_DEFINES := $(CPP_DEFINES) -DOPENGL
endif

ifneq ($(USE_SSE), false)
	AMD64_FLAGS := $(AMD64_FLAGS) -DSSE
endif

Windows:
	@mkdir -p "Build/Windows"AMD64_FLAGS
	$(MAKE) -f make_impl CXX="x86_64-w64-mingw32-g++-posix" \ BUILD_DIR=Build/Windows SOURCE_DIRECTORIES="$(SOURCE_DIRS) src/" INCLUDE_DIR="win32_libraries/include/" LIBRARY_DIR=win32_libraries/lib/ \
	LIBS="opengl32 gdi32 glew32" LDFLAGS_IN="-static -static-libstdc++ -static-libgcc" CXXFLAGS_IN="$(CPP_DEFINES) -mwindows -g3 -municode" USE_DEPFLAGS=false
	  
Debug:
	@mkdir -p "Build/Debug"
	$(MAKE) -f make_impl BUILD_DIR=Build/Debug SOURCE_DIRECTORIES="$(SOURCE_DIRS) src/" LIBRARY_DIR=/usr/local/lib \
	LIBS=" asan ubsan $(LINUX_LIBS)" LDFLAGS_IN="$(AMD64_FLAGS)" CXXFLAGS_IN="$(DEBUG_FLAGS) $(AMD64_FLAGS) $(CPP_DEFINES)"

AMD64:
	@mkdir -p "Build/AMD64"
	$(MAKE) -f make_impl CXX="x86_64-linux-gnu-g++" BUILD_DIR=Build/AMD64 SOURCE_DIRECTORIES="$(SOURCE_DIRS) src/"  LIBRARY_DIR=/usr/local/lib \
	LIBS="$(LINUX_LIBS)" LDFLAGS_IN="$(AMD64_FLAGS)" CXXFLAGS_IN="-O3 -g3 $(AMD64_FLAGS) $(CPP_DEFINES)"

ARM64:
	@mkdir -p "Build/ARM64"
	$(MAKE) -f make_impl CXX="aarch64-linux-gnu-g++" BUILD_DIR=Build/ARM64 SOURCE_DIRECTORIES="$(SOURCE_DIRS) src/"  LIBRARY_DIR=/usr/local/lib \
	LIBS="$(LINUX_LIBS)" LDFLAGS_IN="$(ARM64_FLAGS)" CXXFLAGS_IN="-O3 -g3 $(ARM64_FLAGS) $(CPP_DEFINES)"

Coverage:
	@mkdir -p "Build/Unit_Tests"
	$(MAKE) -f make_impl BUILD_DIR=Build/Unit_Tests TARGET_EXE=test_suite SOURCE_DIRECTORIES="$(TEST_DIRS) $(SOURCE_DIRS)" LIBRARY_DIR=/usr/local/lib \
	LIBS="$(LINUX_LIBS) gcov" CXXFLAGS_IN="--coverage"

clean: 
	rm -rf Build/
	rm -rf game/rpggame
	rm -rf coverage.info
	rm -rf coverage_docs
	rm -rf test_suite

.PHONY: Windows Debug AMD64 ARM64 Coverage clean
