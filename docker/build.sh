#!/bin/bash

IMAGE="$1"

[ "$IMAGE" == "" ] && {
    echo "usage: build.sh <image>"
    exit 1
}

[ -d "docker/$IMAGE" ] || {
    echo "image '$IMAGE' is invalid"
    exit 1
}

docker build -f "docker/$IMAGE/Dockerfile" -t "decentrawise/graphene-$IMAGE:latest" .
