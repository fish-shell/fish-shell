# Currently need testing for rustc>=1.67
FROM debian:testing
LABEL org.opencontainers.image.source=https://github.com/fish-shell/fish-shell

ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
  && apt-get -y install \
    build-essential \
    cargo \
    cmake \
    debhelper \
    debmake \
    file \
    g++ \
    gettext \
    git \
    libpcre2-dev \
    locales \
    locales-all \
    ninja-build \
    pkg-config \
    python3 \
    python3-pexpect \
    python3-launchpadlib \
    rustc \
    sudo \
    tmux \
  && locale-gen en_US.UTF-8 \
  && apt-get clean

RUN groupadd -g 1000 fishuser \
  && useradd -p $(openssl passwd -1 fish) -d /home/fishuser -m -u 1000 -g 1000 fishuser \
  && adduser fishuser sudo \
  && mkdir /fish-source \
  && chown -R fishuser:fishuser /home/fishuser /fish-source

USER fishuser
WORKDIR /home/fishuser

COPY /build-debian-package.sh /

CMD cp -r /gnupg ~/.gnupg && \
        /build-debian-package.sh /fish-source /fish-source/release /home/fishuser/dpkgs || \
        bash
