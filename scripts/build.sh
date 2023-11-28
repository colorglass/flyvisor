#!/bin/bash

set -e

RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

script_path=$(realpath $0)
root_dir="$(cd "$(dirname $script_path)/.." >/dev/null 2>&1 && pwd )"

platform_list="rpi4"
default_platform="rpi4"

app_name=$1
if [ $# -gt 1 ]; then
    platform=$2
elif [ $# -eq 1 ]; then
    platform=$default_platform
else
    echo -e "${YELLOW}Usage${NC}: $0 <app_name> [platform] [\"extra_cmake_args\"]"
    exit 1
fi

app_dir=$root_dir/apps/$app_name
# check if app exists
if [ ! -d "$app_dir" ]; then
    echo -e "${RED}Error${NC}: can not find $app_name under apps directory"
    exit 1
fi

# check if platform is in list
if [[ ! $platform_list =~ (^|[[:space:]])$platform($|[[:space:]]) ]]; then
    echo -e "${YELLOW}Error${NC}: $platform is not a valid platform, set to default: $default_platform"
    platform=$default_platform
fi

build_dir=$root_dir/builds/$app_name
output_dir=$root_dir/outputs/$app_name

echo -e "${GREEN}Info${NC}: building app $app_name for platform $platform"

if [ -d "$build_dir" ]; then
    read -p "Build directory already exists. Do you want to delete it? [y/n]"$'\n' -s -n 1 -r REPLY
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        rm -rf "$build_dir"
        mkdir "$build_dir"
    fi
else
    mkdir -p "$build_dir"
fi

cmake -S $app_dir \
    -B $build_dir \
    -GNinja \
    -DPLATFORM=$platform \
    $3

echo -e "${GREEN}Info${NC}: CMake Configuration complete\n"

ninja -C "$build_dir"

echo -e "${GREEN}Info${NC}: Build complete\n"

if [ ! -d "$output_dir" ]; then
    mkdir -p "$output_dir"
fi

image_name=$(ls "$build_dir/images/")
echo -e "${GREEN}Info${NC}: copy $build_dir/images/$image_name to output"
cp "$build_dir/images/$image_name" "$output_dir" -v

# cp to tftp server dir
if [ -d "/srv/tftp" ]; then
    echo -e "${GREEN}Info${NC}: copy $image_name to /srv/tftp"
    cp "$build_dir/images/$image_name" "/srv/tftp/sel4_image" -v
fi