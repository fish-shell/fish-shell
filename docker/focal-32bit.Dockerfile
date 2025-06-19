FROM ubuntu:20.04
LABEL org.opencontainers.image.source=https://github.com/fish-shell/fish-shell

ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
  && apt-get -y install \
    build-essential \
    g++-multilib \
    gettext \
    git \
    locales \
    pkg-config \
    python3 \
    python3-pexpect \
    sudo \
    tmux \
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

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs > /tmp/rustup.sh \
  && sh /tmp/rustup.sh -y --default-toolchain nightly --component rust-src

COPY fish_run_tests.sh /

ENV \
    CFLAGS=-m32 \
    PCRE2_SYS_STATIC=1 \
    FISH_CHECK_TARGET_TRIPLE=i686-unknown-linux-gnu

ENV FISH_CHECK_LINT=false

CMD . ~/.cargo/env \
    && rustup target add ${FISH_CHECK_TARGET_TRIPLE} \
    && /fish_run_tests.sh
