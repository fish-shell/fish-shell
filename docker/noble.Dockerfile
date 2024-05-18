FROM ubuntu:noble
LABEL org.opencontainers.image.source=https://github.com/fish-shell/fish-shell

ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8

RUN apt-get update \
  && apt-get -y install \
    build-essential \
    cmake \
    gettext \
    git \
    libpcre2-dev \
    locales \
    ninja-build \
    python3 \
    python3-pexpect \
    tmux \
    rustc \
    sudo \
  && locale-gen en_US.UTF-8 \
  && apt-get clean

RUN groupadd -g 1001 fishuser \
  && useradd -p $(openssl passwd -1 fish) -d /home/fishuser -m -u 1001 -g 1001 fishuser \
  && adduser fishuser sudo \
  && mkdir -p /home/fishuser/fish-build \
  && mkdir /fish-source \
  && chown -R fishuser:fishuser /home/fishuser /fish-source

USER fishuser
WORKDIR /home/fishuser

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs > /tmp/rustup.sh \
  && sh /tmp/rustup.sh -y --default-toolchain 1.75

COPY fish_run_tests.sh /

CMD . ~/.cargo/env \
  && /fish_run_tests.sh
