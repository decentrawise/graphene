#!/bin/bash
GRAPHENED="/usr/local/bin/graphened"

# For blockchain download
VERSION=`cat /etc/graphene/version`

## Supported Environment Variables
#
#   * $GRAPHENED_SEED_NODES
#   * $GRAPHENED_RPC_ENDPOINT
#   * $GRAPHENED_PLUGINS
#   * $GRAPHENED_REPLAY
#   * $GRAPHENED_RESYNC
#   * $GRAPHENED_P2P_ENDPOINT
#   * $GRAPHENED_VALIDATOR_ID
#   * $GRAPHENED_BLOCK_PRODUCER_KEYS
#   * $GRAPHENED_TRACK_ACCOUNTS
#   * $GRAPHENED_PARTIAL_OPERATIONS
#   * $GRAPHENED_MAX_OPS_PER_ACCOUNT
#   * $GRAPHENED_ES_NODE_URL
#   * $GRAPHENED_ES_START_AFTER_BLOCK
#   * $GRAPHENED_TRUSTED_NODE
#

ARGS=""
# Translate environment variables
if [[ ! -z "$GRAPHENED_SEED_NODES" ]]; then
    for NODE in $GRAPHENED_SEED_NODES ; do
        ARGS+=" --seed-node=$NODE"
    done
fi
if [[ ! -z "$GRAPHENED_RPC_ENDPOINT" ]]; then
    ARGS+=" --rpc-endpoint=${GRAPHENED_RPC_ENDPOINT}"
fi

if [[ ! -z "$GRAPHENED_REPLAY" ]]; then
    ARGS+=" --replay-blockchain"
fi

if [[ ! -z "$GRAPHENED_RESYNC" ]]; then
    ARGS+=" --resync-blockchain"
fi

if [[ ! -z "$GRAPHENED_P2P_ENDPOINT" ]]; then
    ARGS+=" --p2p-endpoint=${GRAPHENED_P2P_ENDPOINT}"
fi

if [[ ! -z "$GRAPHENED_VALIDATOR_ID" ]]; then
    ARGS+=" --validator-id=$GRAPHENED_VALIDATOR_ID"
fi

if [[ ! -z "$GRAPHENED_BLOCK_PRODUCER_KEYS" ]]; then
    ARGS+=" --block-producer-keys=$GRAPHENED_BLOCK_PRODUCER_KEYS"
fi

if [[ ! -z "$GRAPHENED_TRACK_ACCOUNTS" ]]; then
    for ACCOUNT in $GRAPHENED_TRACK_ACCOUNTS ; do
        ARGS+=" --track-account=$ACCOUNT"
    done
fi

if [[ ! -z "$GRAPHENED_PARTIAL_OPERATIONS" ]]; then
    ARGS+=" --partial-operations=${GRAPHENED_PARTIAL_OPERATIONS}"
fi

if [[ ! -z "$GRAPHENED_MAX_OPS_PER_ACCOUNT" ]]; then
    ARGS+=" --max-ops-per-account=${GRAPHENED_MAX_OPS_PER_ACCOUNT}"
fi

if [[ ! -z "$GRAPHENED_ES_NODE_URL" ]]; then
    ARGS+=" --elasticsearch-node-url=${GRAPHENED_ES_NODE_URL}"
fi

if [[ ! -z "$GRAPHENED_ES_START_AFTER_BLOCK" ]]; then
    ARGS+=" --elasticsearch-start-es-after-block=${GRAPHENED_ES_START_AFTER_BLOCK}"
fi

if [[ ! -z "$GRAPHENED_TRUSTED_NODE" ]]; then
    ARGS+=" --trusted-node=${GRAPHENED_TRUSTED_NODE}"
fi

## Link the graphene config file into home
## This link has been created in Dockerfile, already
ln -f -s /etc/graphene/config.ini /var/lib/graphene
ln -f -s /etc/graphene/logging.ini /var/lib/graphene

chown -R graphene:graphene /var/lib/graphene

# Get the latest security updates
apt-get update && apt-get upgrade -y -o Dpkg::Options::="--force-confold"

# Plugins need to be provided in a space-separated list, which
# makes it necessary to write it like this
if [[ ! -z "$GRAPHENED_PLUGINS" ]]; then
   exec /usr/bin/setpriv --reuid=graphene --regid=graphene --clear-groups \
     "$GRAPHENED" --data-dir "${HOME}" ${ARGS} ${GRAPHENED_ARGS} --plugins "${GRAPHENED_PLUGINS}"
else
   exec /usr/bin/setpriv --reuid=graphene --regid=graphene --clear-groups \
     "$GRAPHENED" --data-dir "${HOME}" ${ARGS} ${GRAPHENED_ARGS}
fi
