FROM ubuntu:22.04
#FROM ubuntu:18.04

WORKDIR /judge
RUN apt update
RUN apt install -y git g++ cmake ninja-build libseccomp-dev libnl-genl-3-dev libsqlite3-dev libz-dev libssl-dev ghc python2 python3 python3-numpy python3-pil libboost-all-dev

# for ubuntu 18
#RUN apt install -y lsb-release gpg software-properties-common wget
#RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
#RUN apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main"
#RUN add-apt-repository ppa:ubuntu-toolchain-r/test
#RUN apt install -y kitware-archive-keyring
#RUN apt install -y git gcc-11 g++-11 cmake ninja-build libseccomp-dev libnl-genl-3-dev libsqlite3-dev libz-dev libssl-dev ghc python python3
#RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 70 --slave /usr/bin/g++ g++ /usr/bin/g++-11 --slave /usr/bin/gcc-nm gcc-nm /usr/bin/gcc-nm-11 --slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-11 --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-11

COPY . ./

WORKDIR /judge/build
RUN cmake -G Ninja ..
RUN ninja && ninja install

WORKDIR /judge

CMD ["/judge/scripts/init.sh", "-v"]
