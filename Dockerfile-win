FROM ubuntu:bionic

ARG cores=8
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
     libevent-dev autoconf automake pkg-config libssl-dev \
     libdb4.8-dev libdb4.8++-dev \
  && apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# mingw
RUN apt update \
  && apt install -y --no-install-recommends \
     g++ mingw-w64 g++-mingw-w64 nsis zip \
  && apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

ENV PROJECTDIR=/opt/blocknetdx/BlockDX
ENV BASEPREFIX=$PROJECTDIR/depends
ENV DISTDIR=/opt/blocknetdx/dist
ENV HOST=x86_64-w64-mingw32

# Download depends
RUN mkdir -p $PROJECTDIR \
  && cd $PROJECTDIR \
  && wget -nv https://s3.us-east-2.amazonaws.com/devbuilds.blocknetprotocol.com/depends/depends-3.11.1-dev-win.tar.gz \
  && [ "$(printf '427e0cfcdf157b797e3ebf8f4c0e29aaee06146ea3b10737efd550a0e9c533ff depends-3.11.1-dev-win.tar.gz' | sha256sum -c)" = "depends-3.11.1-dev-win.tar.gz: OK" ] || $(echo "depends checksum failed"; exit 1)

# Copy source files
#RUN cd /opt/blocknetdx \
#  && git clone --depth 1 --branch 3.12.1 https://github.com/BlocknetDX/blocknet.git tmp \
#  && mv tmp/* BlockDX/
COPY . $PROJECTDIR/

# Build source
RUN mkdir -p $DISTDIR \
  && cd $PROJECTDIR \
  && rm -r $BASEPREFIX \
  && tar xzvf depends-3.11.1-dev-win.tar.gz \
  && chmod +x ./autogen.sh; sync \
  && ./autogen.sh \
  && CONFIG_SITE=$BASEPREFIX/$HOST/share/config.site ./configure CXXFLAGS='-Wno-deprecated' --enable-reduce-exports --disable-bench --disable-ccache --disable-maintainer-mode --disable-dependency-tracking --with-gui=qt5 --prefix=/ \
  && make clean \
  && echo "Building with cores: $ecores" \
  && make -j$ecores \
  && make install DESTDIR=$DISTDIR \
  && make clean

WORKDIR /opt/blocknetdx/dist

# Port, RPC, Test Port, Test RPC
EXPOSE 41412 41414 41474 41419
