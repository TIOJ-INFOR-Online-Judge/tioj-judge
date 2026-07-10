FROM ubuntu:26.04 AS python2-build

WORKDIR /tmp
RUN apt update \
    && DEBIAN_FRONTEND=noninteractive apt install -y build-essential ca-certificates curl \
    && curl -fsSLO https://www.python.org/ftp/python/2.7.18/Python-2.7.18.tgz \
    && echo 'da3080e3b488f648a3d7a4560ddee895284c3380b11d6de75edb986526b9a814  Python-2.7.18.tgz' | sha256sum -c - \
    && tar -xzf Python-2.7.18.tgz \
    && cd Python-2.7.18 \
    && CC='gcc -std=gnu17' CXX='g++ -std=gnu++17' ./configure --prefix=/opt/python2 --without-ensurepip --disable-shared \
    && make -j"$(nproc)" \
    && make install \
    && rm -rf /var/lib/apt/lists/*

FROM ubuntu:26.04

WORKDIR /judge
RUN apt update \
    && DEBIAN_FRONTEND=noninteractive apt install -y git g++ cmake ninja-build libseccomp-dev libnl-genl-3-dev libsqlite3-dev libz-dev libssl-dev libjitterentropy3-dev ghc python3 python3-numpy python3-pil libboost-all-dev libzstd-dev rustc \
    && rm -rf /var/lib/apt/lists/*

COPY --from=python2-build /opt/python2 /opt/python2
RUN ln -s /opt/python2/bin/python2.7 /usr/local/bin/python2

COPY . ./

RUN cmake -B build -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5
RUN cmake --build build
RUN cmake --install build

WORKDIR /judge

CMD ["/judge/scripts/init.sh", "-v"]
