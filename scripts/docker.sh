#!/bin/sh

RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

if [ ! -x $(command -v make)]; then
    echo -e "${RED}Error${NC}: make is not installed."
    exit 1
fi

# check if docker was installed
if [ ! -x $(command -v docker) ]; then
    echo -e "${RED}Error${NC}: docker is not installed."
    exit 1
fi

# check current user is in the docker group
if ! groups | grep -qw docker; then
    echo -e "${RED}Error${NC}: current user is not in the docker group."
    echo -e "${YELLOW}Info${NC}: run 'sudo usermod -aG docker $USER' to add current user to the docker group."
    exit 1
fi

root_dir=$(dirname $0)/..
cd ${root_dir}/docker

# build docker image
echo -e "${GREEN}Info${NC}: build docker image"
make build_user

echo $"alias container=\'make -C \$(pwd) user HOST_DIR=\$(pwd)\'" >> ~/.profile
source ~/.profile
echo -e "${GREEN}Info${NC}: run 'container' to enter the docker container"