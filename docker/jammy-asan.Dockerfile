FROM ubuntu:jammy
LABEL org.opencontainers.image.source=https://github.com/fish-shell/fish-shell

ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8

RUN apt-get update \
  && apt-get -y install \
    build-essential \
    cmake \
    clang \
    gettext \
    git \
    libpcre2-dev \
    locales \
    ninja-build \
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

RUN llvm_version=$(clang --version | awk 'NR==1 { split($NF, version, "."); print version[1] }') \
    && echo "$llvm_version" >/.llvm-version

USER fishuser
WORKDIR /home/fishuser

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs > /tmp/rustup.sh \
  && sh /tmp/rustup.sh -y --default-toolchain nightly --component rust-src

COPY fish_run_tests.sh /

ENV \
    RUSTFLAGS=-Zsanitizer=address \
    RUSTDOCFLAGS=-Zsanitizer=address \
    CC=clang \
    CXX=clang++ \
    ASAN_OPTIONS=check_initialization_order=1:detect_stack_use_after_return=1:detect_leaks=1 \
    LSAN_OPTIONS=verbosity=0:log_threads=0:use_tls=1:print_suppressions=0:suppressions=/fish-source/build_tools/lsan_suppressions.txt \
    FISH_CI_SAN=1

CMD . ~/.cargo/env \
  && ASAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer-$(cat /.llvm-version) \
       /fish_run_tests.sh -DASAN=1 -DRust_CARGO_TARGET=x86_64-unknown-linux-gnu
