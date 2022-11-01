FROM ubuntu:16.04
LABEL org.opencontainers.image.source=https://github.com/fish-shell/fish-shell

ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8

RUN apt-get update \
  && apt-get -y install \
    build-essential \
    cmake \
    gettext \
    git \
    libncurses5-dev \
    libpcre2-dev \
    locales \
    ninja-build \
    python3 \
    python3-pip \
    sudo \
  && locale-gen en_US.UTF-8 \
  && apt-get clean

# The python3-pexpect package on Xenial doesn't allow delaybeforesend to be None.
# Install pexpect with pip which is newer.
RUN pip3 install pexpect

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
