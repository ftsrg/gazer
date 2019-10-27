FROM ubuntu:18.04

RUN apt-get update && \
    apt-get install -y build-essential git cmake \
    wget sudo vim lsb-release \
    software-properties-common zlib1g-dev

# fetch LLVM and other dependencies
RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - && \
    add-apt-repository "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-9 main" && \
    apt-get update && \
    add-apt-repository ppa:mhier/libboost-latest && \
    apt-get update && \
    apt-get install -y clang-9 llvm-9-dev llvm-9-tools llvm-9-runtime libboost1.68-dev && \
    ln -s `which clang-9` /usr/bin/clang && \
    ln -s `which llvm-link-9` /usr/bin/llvm-link

# create a new user `user` with the password `user` and sudo rights
RUN useradd -m user && \
    echo user:user | chpasswd && \
    cp /etc/sudoers /etc/sudoers.bak && \
    echo 'user  ALL=(root) NOPASSWD: ALL' >> /etc/sudoers

USER user

ENV GAZER_DIR /home/user/gazer

ADD --chown=user:user . $GAZER_DIR

WORKDIR $GAZER_DIR
RUN cmake -DCMAKE_CXX_COMPILER=clang++-9 -DGAZER_ENABLE_UNIT_TESTS=On -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=On . && make
