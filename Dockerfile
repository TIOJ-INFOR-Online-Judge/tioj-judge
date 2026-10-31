FROM ubuntu:26.04

WORKDIR /judge
RUN apt update
RUN apt install -y \
    software-properties-common git g++ cmake ninja-build curl \
    libseccomp-dev libnl-genl-3-dev libsqlite3-dev libbsd-dev libjitterentropy3-dev \
    libz-dev libbz2-dev libssl-dev libboost-all-dev libzstd-dev
RUN add-apt-repository -y ppa:jdxcode/mise && \
    apt install -y mise
RUN mise plugin add ghc
# ghc install sometimes fail
RUN mise install ghc@9.10 || mise install ghc@9.10
RUN mise install python@2.7 python@3.14 rust@1.93

ENV PATH="/root/.local/share/mise/installs/rust/1.93:/root/.local/share/mise/installs/python/3.14/bin:/root/.local/share/mise/installs/python/2.7/bin:/root/.local/share/mise/installs/ghc/9.10/bin:${PATH}"
RUN pip3 install numpy~=2.5 Pillow~=13.0

COPY . ./

RUN apt install -y libbsd-dev
RUN cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -B build -G Ninja
RUN cmake --build build
RUN cmake --install build

WORKDIR /judge

CMD ["/judge/scripts/init.sh", "-v"]
