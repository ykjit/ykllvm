#!/bin/sh
#
# Format yk-specific folders.
#
# In the past we tried using git-clang-format against upstream LLVM. This
# worked well until you do an upstream sync. Since upstream's changes are not
# consistently formatted, it meant that CI would always fail.
#
# For now we just format folders that contain only new files that we've
# introduced for Yk.
#
# This script must be run in the root of the repository.

set -e

# Directories where every file is yk-specific and should be formatted.
YK_DIRS="./clang/test/Yk ./llvm/lib/Transforms/Yk ./llvm/include/llvm/Transforms/Yk llvm/lib/YkIR ./llvm/lib/CodeGen/Yk"

# Individual files that are yk-specific and should be formatted.
YK_FILES="./llvm/include/llvm/ADT/YkDisjointLocationSets.h"

if ./build/bin/clang-format -version > /dev/null 2>&1; then
    clang_format=./build/bin/clang-format
elif ../target/release/ykllvm/bin/clang-format -version > /dev/null 2>&1; then
    clang_format=../target/release/ykllvm/bin/clang-format
elif ../target/debug/ykllvm/bin/clang-format -version > /dev/null 2>&1; then
    clang_format=../target/debug/ykllvm/bin/clang-format
else
    echo "Can't find clang-format" > /dev/null
    exit 1
fi
clang_format=$(readlink -f "$clang_format")
echo "Using $clang_format"

for dir in ${YK_DIRS}; do
    find ${dir} -type f -iname '*.cpp' -or -iname '*.h' -or -iname '*.c' | \
        xargs ${clang_format} -i
done

for f in ${YK_FILES}; do
        ${clang_format} -i $f
done
