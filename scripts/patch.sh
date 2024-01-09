#!/bin/sh
set -e

root_dir=$(dirname $0)/..
patch_dir=$(dirname $0)/../patches

for patch in $(find $patch_dir -name "*.patch"); do
    relative_path=${patch#$patch_dir/}
    repo=${root_dir}/$(dirname $relative_path)
    patch -d $repo -p1 -N < $patch
done