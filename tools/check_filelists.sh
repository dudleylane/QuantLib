#!/bin/bash

# get reference lists of existing files (done with find)

find ql -name '*.[hc]pp' -or -name '*.[hc]' \
| grep -v 'ql/config\.hpp' | sort > ql.ref.files
find test-suite -name '*.[hc]pp' \
| grep -v 'quantlibbenchmark' | grep -v '/main\.cpp' \
| sort > test-suite.ref.files

# get list of distributed files from packaged tarball

make dist

mkdir dist-check
mv QuantLib-*.tar.gz dist-check
cd dist-check
tar xfz QuantLib-*.tar.gz
rm QuantLib-*.tar.gz
cd QuantLib-*

find ql -name '*.[hc]pp' \
| grep -v 'ql/config\.hpp' | sort > ../../ql.dist.files
find test-suite -name '*.[hc]pp' \
| grep -v 'quantlibbenchmark' | grep -v '/main\.cpp' \
| sort > ../../test-suite.dist.files

cd ../..
rm -rf dist-check

# MSVC vcxproj drift checks removed -- the fork supports RHEL/CentOS 10+
# only (memory: project_supported_platforms.md).

# same with CMakelists

# reference files above align only to autotools build system
grep -o -E '[a-zA-Z0-9_/\.]*\.[hc]pp' ql/CMakeLists.txt \
| grep -v 'ql/config\.hpp' | sed -e 's|/ql/||' | sed -e 's|^|ql/|' | sort > ql.cmake.files

grep -o -E '[a-zA-Z0-9_/\.]*\.[hc]pp' test-suite/CMakeLists.txt \
| grep -v 'quantlibbenchmark' | grep -v 'main\.cpp' \
| sed -e 's|^|test-suite/|' | sort -u > test-suite.cmake.files

# write out differences...

diff -b ql.dist.files ql.ref.files > ql.dist.diff
diff -b test-suite.dist.files test-suite.ref.files > test-suite.dist.diff

diff -b ql.cmake.files ql.ref.files > ql.cmake.diff
diff -b test-suite.cmake.files test-suite.ref.files > test-suite.cmake.diff

# ...process...
./tools/check_filelists_diffs.py
result=$?

# ...and cleanup
rm -f ql.*.files test-suite.*.files
rm -f ql.*.diff test-suite.*.diff

exit $result
