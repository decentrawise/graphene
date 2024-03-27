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

ADD . /graphene
WORKDIR /graphene

# Compile
RUN \
  git submodule deinit --all && \
  git submodule update --init --recursive && \
  cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DGRAPHENE_DISABLE_UNITY_BUILD=ON \
    . && \
  make witness_node cli_wallet get_dev_key && \
  install -s programs/witness_node/witness_node \
    programs/genesis_util/get_dev_key \
    programs/cli_wallet/cli_wallet \
    /usr/local/bin && \
  #
  # Obtain version
  mkdir -p /etc/graphene && \
  git rev-parse --short HEAD > /etc/graphene/version && \
  cd / && \
  rm -rf /graphene

# The final image
FROM phusion/baseimage:focal-1.2.0
LABEL maintainer="DecentraWise Blockchain Organisation"
ENV LANG=en_US.UTF-8

# Install required libraries
RUN \
  apt-get update && \
  apt-get upgrade -y -o Dpkg::Options::="--force-confold" && \
  apt-get update && \
  apt-get install --no-install-recommends -y \
  libcurl4 \
  ca-certificates \
  && \
  mkdir -p /etc/graphene && \
  apt-get clean && \
  rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

COPY --from=build /usr/local/bin/* /usr/local/bin/
COPY --from=build /etc/graphene/version /etc/graphene/

WORKDIR /
RUN groupadd -g 10000 graphene
RUN useradd -u 10000 -g graphene -s /bin/bash -m -d /var/lib/graphene --no-log-init graphene
ENV HOME /var/lib/graphene
RUN chown graphene:graphene -R /var/lib/graphene

# default exec/config files
ADD docker/config/default_config.ini /etc/graphene/config.ini
ADD docker/config/default_logging.ini /etc/graphene/logging.ini
ADD docker/graphene/graphene-entry.sh /usr/local/bin/graphene-entry.sh
RUN chmod a+x /usr/local/bin/graphene-entry.sh

# Volume
VOLUME ["/var/lib/graphene", "/etc/graphene"]

# rpc service:
EXPOSE 8090
# p2p service:
EXPOSE 1776

# Make Docker send SIGINT instead of SIGTERM to the daemon
STOPSIGNAL SIGINT

# Temporarily commented out due to permission issues caused by older versions, to be restored in a future version
# USER graphene:graphene

# default execute entry
CMD ["/usr/local/bin/graphene-entry.sh"]
