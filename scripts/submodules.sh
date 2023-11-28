#!/bin/sh

# get path of current executing script

git_modules=$(dirname $0)/../.gitmodules

if [ ! -r $git_modules ]; then
    echo "Could not find .gitmodules file"
    exit 1
fi

while IFS= read -r line
do
    if [[ $line == \[submodule* ]]; then
        read -r pathLine
        read -r urlLine
        path=${pathLine#path = }
        url=${urlLine#url = }
        git submodule add "$url" "$path"
    fi
done < .gitmodules