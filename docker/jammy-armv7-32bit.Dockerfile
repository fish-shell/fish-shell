FROM arm32v7/ubuntu:jammy
LABEL org.opencontainers.image.source=https://github.com/fish-shell/fish-shell

ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8
ENV CXXFLAGS="-Werror=address -Werror=return-type -Wno-psabi"
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
  && apt-get -y install \
    build-essential \
    cmake \
    file \
    g++ \
    gettext \
    git \
    libncurses5-dev \
    libpcre2-dev \
    locales \
    ninja-build \
    pkg-config \
    python3 \
    python3-pexpect \
    sudo \
  && locale-gen en_US.UTF-8 \
  && apt-get clean

RUN groupadd -g 1000 fishuser \
  && useradd -p $(openssl passwd -1 fish) -d /home/fishuser -m -u 1000 -g 1000 fishuser \
  && adduser fishuser sudo \
  && mkdir -p /home/fishuser/fish-build \
  && mkdir /fish-source \
  && chown -R fishuser:fishuser /home/fishuser /fish-source

USER fishuser
WORKDIR /home/fishuser

COPY fish_run_tests.sh /


CMD /fish_run_tests.sh
