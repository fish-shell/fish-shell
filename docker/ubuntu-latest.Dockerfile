FROM ubuntu:latest
LABEL org.opencontainers.image.source=https://github.com/fish-shell/fish-shell

ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

RUN apt-get update \
  && apt-get -y install --no-install-recommends  \
    cmake ninja-build \
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

RUN userdel ubuntu \
  && groupadd -g 1000 fishuser \
  && useradd -p $(openssl passwd -1 fish) -d /home/fishuser -m -u 1000 -g 1000 fishuser \
       -G sudo \
  && mkdir -p /home/fishuser/fish-build \
  && mkdir /fish-source \
  && chown -R fishuser:1000 /home/fishuser /fish-source

USER fishuser
WORKDIR /home/fishuser

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs > /tmp/rustup.sh \
  && sh /tmp/rustup.sh -y --no-modify-path
ENV PATH=/home/fishuser/.cargo/bin:$PATH

COPY fish_run_tests.sh /

ENV FISH_CHECK_LINT=false

CMD /fish_run_tests.sh
