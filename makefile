SOURCE_DIRS := src/common/ src/ecs/ src/engine/ src/renderer/ src/main/
TEST_DIRS := tests/


Windows:
	@mkdir -p "Build/Windows"
	$(MAKE) -f make_impl CXX="x86_64-w64-mingw32-g++-posix" \
	BUILD_DIR=Build/Windows SOURCE_DIRECTORIES="$(SOURCE_DIRS) src/" INCLUDE_DIR="win32_libraries/include/" LIBRARY_DIR=win32_libraries/lib/ \
	LIBS="opengl32 gdi32 glew32" LDFLAGS_IN="-static -static-libstdc++ -static-libgcc" CXXFLAGS_IN="-mwindows -g3 -municode" \
	  
Debug: 
	@mkdir -p "Build/Debug"
	$(MAKE) -f make_impl \
	BUILD_DIR=Build/Debug \	SOURCE_DIRECTORIES="$(SOURCE_DIRS) src/" LIBRARY_DIR=/usr/local/lib \
	LIBS="asan Xext ubsan freetype GLEW GL X11" CXXFLAGS_IN="-Wno-unused-parameter -fsanitize=undefined -fsanitize=address -g3 -Wall -Wextra" DEPFLAGS_IN="-M -MMD -MP"
	  
Release:
	@mkdir -p "Build/Release"
	$(MAKE) -f make_impl \
	BUILD_DIR=Build/Release SOURCE_DIRECTORIES="$(SOURCE_DIRS) src/"  LIBRARY_DIR=/usr/local/lib \
	LIBS="freetype Xext GLEW GL X11" CXXFLAGS_IN="-O3 -g3" DEPFLAGS_IN="-M -MMD -MP"

Coverage:
	@mkdir -p "Build/Unit_Tests"
	$(MAKE) -f make_impl BUILD_DIR=Build/Unit_Tests TARGET_EXE=test_suite SOURCE_DIRECTORIES="$(TEST_DIRS) $(SOURCE_DIRS)"  LIBS="freetype GLEW GL X11" CXXFLAGS_IN="--coverage" LIBRARY_DIR=/usr/local/lib

clean: 
	rm -rf Build/
	rm -rf game/rpggame
	rm -rf coverage.info
	rm -rf coverage_docs
	rm -rf test_suite

.PHONY: Windows Debug Release Coverage_Docs clean
