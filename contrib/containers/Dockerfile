# syntax=docker/dockerfile:1.3
FROM ubuntu:bionic
LABEL maintainer="Blocknet Developers <dev@blocknet.org>"
LABEL description="Dockerised Blocknet"

ARG TAG

RUN apt-get update && \
    apt-get -y install --no-install-recommends \
    wget \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /opt/blockchain && \
    mkdir -p /opt/blockchain/data && \
    mkdir -p /opt/blockchain/config && \
    ln -s /opt/blockchain/config /root/.blocknet

COPY blocknet-binaries/**/blocknet*64-linux-gnu.tar.gz /opt/blockchain

RUN mach=$(uname -m) \
    && case $mach in aarch64) arch="aarch64-linux-gnu"; ;; x86_64) arch="x86_64-linux-gnu"; ;;  *) echo "ERROR: Machine type $mach not supported."; ;; esac \
    && cd /opt/blockchain \
    && tar xvzf blocknet*$arch.tar.gz \
    && echo $(ls -1 /opt/blockchain) \
    && cp blocknet-*/bin/* /usr/local/bin \
    && rm -rf blocknet*

# Write default blocknet.conf (can be overridden on commandline)
RUN echo "datadir=/opt/blockchain/data    \n\
                                          \n\
maxmempoolxbridge=128                     \n\
                                          \n\
port=41412    # testnet: 41474            \n\
rpcport=41414 # testnet: 41419            \n\
                                          \n\
listen=1                                  \n\
txindex=1                                 \n\
server=1                                  \n\
logtimestamps=1                           \n\
logips=1                                  \n\
                                          \n\
rpcbind=0.0.0.0                           \n\
rpcallowip=127.0.0.1                      \n\
rpctimeout=60                             \n\
rpcclienttimeout=30" > /opt/blockchain/config/blocknet.conf

WORKDIR /opt/blockchain/
VOLUME ["/opt/blockchain/config", "/opt/blockchain/data"]

# Port, RPC, Test Port, Test RPC
EXPOSE 41412 41414 41474 41419

CMD ["blocknetd", "-daemon=0"]

