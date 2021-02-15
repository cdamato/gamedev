make -j12 Coverage
./test_suite
lcov -c --no-external --base-directory $(pwd)  --output-file  coverage.info\
  --directory Build/Unit_Tests/src/renderer/\
  --directory Build/Unit_Tests/src/main/\
  --directory Build/Unit_Tests/src/engine/\
  --directory Build/Unit_Tests/src/ecs/\
  --directory Build/Unit_Tests/src/common/
genhtml coverage.info --output-directory coverage_docs