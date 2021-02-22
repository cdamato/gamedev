SOURCE_DIRS := src/common/ src/ecs/ src/engine/ src/renderer/ src/main/
TEST_DIRS := tests/


Windows:
	@mkdir -p "Build/Windows"
	$(MAKE) -f make_impl BUILD_DIR=Build/Windows CXX="x86_64-w64-mingw32-g++-posix" SOURCE_DIR="src" LIBS="mingw32 opengl32 gdi32 glew32" LDFLAGS_IN="-static" CXXFLAGS_IN="-g3 -municode" INCLUDE_DIR="/home/caroline/win32_includes/include/" LIBRARY_DIR=/home/caroline/win32_includes/lib/
	  
Debug: 
	@mkdir -p "Build/Debug"
	$(MAKE) -f make_impl BUILD_DIR=Build/Debug SOURCE_DIRECTORIES="$(SOURCE_DIRS) src/" LIBS="asan ubsan  freetype GLEW GL X11" CXXFLAGS_IN="-Wno-unused-parameter -fsanitize=undefined -fsanitize=address -g3 -Wall -Wextra" LIBRARY_DIR=/usr/local/lib
	  
Release:
	@mkdir -p "Build/Release"
	$(MAKE) -f make_impl BUILD_DIR=Build/Release SOURCE_DIRECTORIES="$(SOURCE_DIRS) src/" LIBS="freetype GLEW GL X11" CXXFLAGS_IN="-O3" LIBRARY_DIR=/usr/local/lib

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