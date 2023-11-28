#!/bin/sh
set -e

RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

# update submodules
# check if git was installed
if ! [ -x "$(command -v git)" ]; then
    echo -e "${RED}Error${NC}: git is not installed."
    exit 1
fi

root_dir=$(cd $(dirname $(realpath $0))/.. && pwd)
cd $root_dir

echo -e "${GREEN}Info${NC}: update git submodules"
git submodule init
git submodule update

# patching
echo -e "${GREEN}Info${NC}: apply patches"
sh $root_dir/scripts/patch.sh