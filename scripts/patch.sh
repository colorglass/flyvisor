#!/bin/sh
set -e

patch_dir=$(dirname $0)/../patches
seL4_dir=$(dirname $0)/../seL4

for patch in $(find $patch_dir -name "*.patch"); do
    relative_path=${patch#$patch_dir/}
    repo=${seL4_dir}/$(dirname $relative_path)
    patch -d $repo -p1 -N < $patch
done