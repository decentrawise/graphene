#!/bin/bash

IMAGE="$1"

[ "$IMAGE" == "" ] && {
    echo "usage: run.sh <image>"
    exit 1
}

[ -d "docker/$IMAGE" ] || {
    echo "image '$IMAGE' is invalid"
    exit 1
}

docker exec -ti "graphene-$IMAGE" /bin/bash
