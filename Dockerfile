# Build: docker build --build-arg cores=8 -t blocknetdx/devbuilds:latest .
FROM ubuntu:bionic

ARG cores=1
ENV ecores=$cores

RUN apt update \
  && apt install -y --no-install-recommends \
     software-properties-common \
     ca-certificates \
     wget curl git python vim \
  && apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

RUN add-apt-repository ppa:bitcoin/bitcoin \
  && apt update \
  && apt install -y --no-install-recommends \
     build-essential libtool autotools-dev bsdmainutils \
     libevent-dev autoconf automake pkg-config libssl1.0-dev \
     libboost-system-dev libboost-filesystem-dev libboost-chrono-dev \
     libboost-program-options-dev libboost-test-dev libboost-thread-dev \
     libdb4.8-dev libdb4.8++-dev libgmp-dev libminiupnpc-dev libzmq3-dev \
  && apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# gcc8
RUN apt update \
  && apt install -y --no-install-recommends \
     g++-8-multilib gcc-8-multilib binutils-gold \
  && apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

ENV PROJECTDIR=/opt/blocknetdx/BlockDX
ENV BASEPREFIX=$PROJECTDIR/depends
ENV DISTDIR=/opt/blocknetdx/dist
ENV HOST=x86_64-pc-linux-gnu

# Download depends
RUN mkdir -p $PROJECTDIR

# Copy source files
#RUN cd /opt/blocknetdx \
#  && git clone --depth 1 --branch 3.12.1 https://github.com/BlocknetDX/BlockDX.git tmp \
#  && mv tmp/* BlockDX/
COPY . $PROJECTDIR/

# Build source
RUN mkdir -p $DISTDIR \
  && cd $PROJECTDIR \
  && chmod +x ./autogen.sh; sync \
  && ./autogen.sh \
  && ./configure CC=gcc-8 CXX=g++-8 --without-gui --enable-debug --prefix=/ \
  && make clean \
  && echo "Building with cores: $ecores" \
  && make -j$ecores \
  && make install DESTDIR=$DISTDIR \
  && cp $DISTDIR/bin/* /usr/local/bin \
  && make clean \
  && rm -rf /opt/blocknetdx

WORKDIR /opt/blockchain/data

###################
# Blocknet daemon
###################

RUN mkdir -p /opt/blockchain/config \
  && mkdir -p /opt/blockchain/data \
  && ln -s /opt/blockchain/config /root/.blocknetdx

# Write default blocknetdx.conf (can be overridden on commandline)
RUN echo "testnet=1                       \n\
datadir=/opt/blockchain/data              \n\
                                          \n\
dbcache=128                               \n\
maxmempool=256                            \n\
                                          \n\
port=41474    # mainnet: 41412            \n\
rpcport=41419 # mainnet: 41414            \n\
                                          \n\
listen=1                                  \n\
server=1                                  \n\
logtimestamps=1                           \n\
logips=1                                  \n\
enableaccounts=1                          \n\
staking=0                                 \n\
                                          \n\
rpcuser=test                              \n\
rpcpassword=user                          \n\
rpcallowip=0.0.0.0/0                      \n\
rpctimeout=15                             \n\
rpcclienttimeout=15" > /opt/blockchain/config/blocknetdx.conf

VOLUME ["/opt/blockchain/config", "/opt/blockchain/data"]

# Port, RPC, Test Port, Test RPC
EXPOSE 41412 41414 41474 41419

CMD ["blocknetdxd", "-daemon=0"]