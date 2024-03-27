# The image for building graphene
FROM phusion/baseimage:focal-1.2.0 as build
ENV LANG=en_US.UTF-8

# Install dependencies
RUN \
  apt-get update && \
  apt-get upgrade -y -o Dpkg::Options::="--force-confold" && \
  apt-get update && \
  apt-get install -y \
  g++ \
  autoconf \
  cmake \
  git \
  libbz2-dev \
  libcurl4-openssl-dev \
  libssl-dev \
  libncurses-dev \
  libboost-thread-dev \
  libboost-iostreams-dev \
  libboost-date-time-dev \
  libboost-system-dev \
  libboost-filesystem-dev \
  libboost-program-options-dev \
  libboost-chrono-dev \
  libboost-test-dev \
  libboost-context-dev \
  libboost-regex-dev \
  libboost-coroutine-dev \
  libtool \
  doxygen \
  ca-certificates \
  && \
  apt-get clean && \
  rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

COPY . /graphene
WORKDIR /graphene

# Update submodules and build system
RUN \
  git submodule deinit --all && \
  git submodule update --init --recursive && \
  rm -rf build && mkdir build && cd build && \
  cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DGRAPHENE_DISABLE_UNITY_BUILD=ON \
    ..

# Volume
VOLUME ["/graphene/libraries/fc", "/graphene/build"]

# rpc service:
EXPOSE 8090
# p2p service:
EXPOSE 1776

# Make Docker send SIGINT instead of SIGTERM to the daemon
STOPSIGNAL SIGINT
