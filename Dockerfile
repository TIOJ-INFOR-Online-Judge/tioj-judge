FROM ubuntu:26.04

RUN apt update -y && apt upgrade -y
RUN apt install -y \
    software-properties-common git g++ cmake ninja-build curl \
    libseccomp-dev libnl-genl-3-dev libsqlite3-dev libbsd-dev libgmp-dev \
    libz-dev libbz2-dev libreadline-dev libssl-dev libboost-all-dev libzstd-dev \
    libjitterentropy3-dev
RUN add-apt-repository -y ppa:jdxcode/mise && \
    apt install -y mise

RUN chmod 711 /root
RUN mise plugin add ghc
# ghc install sometimes fails
RUN mise install ghc@9.10 || mise install ghc@9.10
RUN mise install python@2.7 python@3.14 rust@1.93
RUN mise exec python@3.14 -- pip3 install numpy~=2.5.0 Pillow~=13.0.0

ENV PATH="/root/.local/share/mise/installs/rust/1.93:/root/.local/share/mise/installs/python/3.14/bin:/root/.local/share/mise/installs/python/2.7/bin:/root/.local/share/mise/installs/ghc/9.10/bin:${PATH}"
ENV TIOJ_JUDGE_MOUNTS="/root/.local/share/mise:/root/.cargo:/root/.rustup"
ENV CARGO_HOME="/root/.cargo"
ENV RUSTUP_HOME="/root/.rustup"

WORKDIR /judge
COPY . ./

RUN cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -B build -G Ninja
RUN cmake --build build
RUN cmake --install build

CMD ["/judge/scripts/init.sh", "-v"]
