# Docker Container

This repository comes with built-in Dockerfile to support docker
containers. This README serves as documentation.

## Dockerfile Specifications

The `Dockerfile` performs the following steps:

1. Obtain base image (phusion/baseimage:0.10.1)
2. Install required dependencies using `apt-get`
3. Add Graphene source code into container
4. Update git submodules
5. Perform `cmake` with build type `Release`
6. Run `make` and `make_install` (this will install binaries into `/usr/local/bin`
7. Purge source code off the container
8. Add a local graphene user and set `$HOME` to `/var/lib/graphene`
9. Make `/var/lib/graphene` and `/etc/graphene` a docker *volume*
10. Expose ports `8090` and `1776`
11. Add default config from `docker/default_config.ini` and
    `docker/default_logging.ini`
12. Add an entry point script
13. Run the entry point script by default

The entry point simplifies the use of parameters for the `witness_node`
(which is run by default when spinning up the container).

### Supported Environmental Variables

* `$GRAPHENED_SEED_NODES`
* `$GRAPHENED_RPC_ENDPOINT`
* `$GRAPHENED_PLUGINS`
* `$GRAPHENED_REPLAY`
* `$GRAPHENED_RESYNC`
* `$GRAPHENED_P2P_ENDPOINT`
* `$GRAPHENED_WITNESS_ID`
* `$GRAPHENED_PRIVATE_KEY`
* `$GRAPHENED_TRACK_ACCOUNTS`
* `$GRAPHENED_PARTIAL_OPERATIONS`
* `$GRAPHENED_MAX_OPS_PER_ACCOUNT`
* `$GRAPHENED_ES_NODE_URL`
* `$GRAPHENED_TRUSTED_NODE`

### Default config

The default configuration is:

    p2p-endpoint = 0.0.0.0:1776
    rpc-endpoint = 0.0.0.0:8090
    history-per-size = 1000
    max-ops-per-account = 100
    partial-operations = true

# Docker Compose

With docker compose, multiple nodes can be managed with a single
`docker-compose.yaml` file:

    version: '3'
    services:
     main:
      # Image to run
      image: decentrawise/graphene:latest
      # 
      volumes:
       - ./docker/conf/:/etc/graphene/
      # Optional parameters
      environment:
       - GRAPHENED_ARGS=--help


    version: '3'
    services:
     fullnode:
      # Image to run
      image: decentrawise/graphene:latest
      environment:
      # Optional parameters
      environment:
       - GRAPHENED_ARGS=--help
      ports:
       - "0.0.0.0:8090:8090"
      volumes:
      - "graphene-fullnode:/var/lib/graphene"


# Docker Hub

This container is properly registered with docker hub under the name:

* [decentrawise/graphene](https://hub.docker.com/r/decentrawise/graphene/)

Going forward, every release tag as well as all pushes to `develop` and
`testnet` will be built into ready-to-run containers, there.

# Docker Compose

One can use docker compose to setup a trusted full node together with a
delayed node like this:

```
version: '3'
services:

 fullnode:
  image: decentrawise/graphene:latest
  ports:
   - "0.0.0.0:8090:8090"
  volumes:
  - "graphene-fullnode:/var/lib/graphene"

 delayed_node:
  image: decentrawise/graphene:latest
  environment:
   - 'GRAPHENED_PLUGINS=delayed_node witness'
   - 'GRAPHENED_TRUSTED_NODE=ws://fullnode:8090'
  ports:
   - "0.0.0.0:8091:8090"
  volumes:
  - "graphene-delayed_node:/var/lib/graphene"
  links: 
  - fullnode

volumes:
 graphene-fullnode:
```
