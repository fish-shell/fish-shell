FROM ubuntu:jammy
LABEL org.opencontainers.image.source=https://github.com/fish-shell/fish-shell

ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

RUN apt-get update \
  && apt-get -y install --no-install-recommends  \
    build-essential \
    ca-certificates \
    clang \
    curl \
    gettext \
    git \
    libpcre2-dev \
    locales \
    openssl \
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
    RUSTFLAGS=-Zsanitizer=thread \
    RUSTDOCFLAGS=-Zsanitizer=thread \
    FISH_CHECK_CARGO_ARGS='-Zbuild-std --features=tsan' \
    FISH_CHECK_TARGET_TRIPLE=x86_64-unknown-linux-gnu \
    FISH_CI_SAN=1 \
    FISH_TEST_MAX_CONCURRENCY=4

ENV FISH_CHECK_LINT=false

CMD . ~/.cargo/env && /fish_run_tests.sh
