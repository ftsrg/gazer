FROM ubuntu:20.04

ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Budapest
ENV THETA_VERSION v2.10.0

RUN apt-get update && \
    apt-get install -y build-essential git cmake \
    wget sudo vim lsb-release \
    software-properties-common zlib1g-dev \
    openjdk-11-jre

# fetch LLVM and other dependencies
RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - && \
    add-apt-repository "deb http://apt.llvm.org/focal/ llvm-toolchain-focal-12 main" && \
    apt-get update && \
    apt-get update && \
    apt-get install -y clang-12 llvm-12-dev llvm-12-tools llvm-12-runtime perl libyaml-tiny-perl

# create a new user `user` with the password `user` and sudo rights
RUN useradd -m user && \
    echo user:user | chpasswd && \
    cp /etc/sudoers /etc/sudoers.bak && \
    echo 'user  ALL=(root) NOPASSWD: ALL' >> /etc/sudoers
    
# (the portfolio uses clang)
RUN ln -sf /usr/bin/clang-9 /usr/bin/clang

USER user

ENV GAZER_DIR /home/user/gazer

ADD --chown=user:user . $GAZER_DIR

WORKDIR $GAZER_DIR
RUN cmake -DCMAKE_CXX_COMPILER=clang++-12 -DGAZER_ENABLE_UNIT_TESTS=On -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=On . && make

# download theta (and libs)
RUN mkdir $GAZER_DIR/tools/gazer-theta/theta && \
    mkdir $GAZER_DIR/tools/gazer-theta/theta/lib && \
    wget "https://github.com/ftsrg/theta/releases/download/$THETA_VERSION/theta-cfa-cli.jar" -O $GAZER_DIR/tools/gazer-theta/theta/theta-cfa-cli.jar && \
    wget "https://github.com/ftsrg/theta/raw/$THETA_VERSION/lib/libz3.so" -P $GAZER_DIR/tools/gazer-theta/theta/lib/ && \
    wget "https://github.com/ftsrg/theta/raw/$THETA_VERSION/lib/libz3java.so" -P $GAZER_DIR/tools/gazer-theta/theta/lib/
