lcov -c --directory Test/renderer/ --directory Test/main  --directory Test/engine/ --directory Test/ecs/ --directory Test/common/ --output-file  coverage.info
genhtml coverage.info --output-directory coverage_docs
