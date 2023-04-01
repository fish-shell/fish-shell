FROM ubuntu:20.04
LABEL org.opencontainers.image.source=https://github.com/ghoti-shell/ghoti-shell

ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8
ENV CXXFLAGS="-m32 -Werror=address -Werror=return-type" CFLAGS="-m32"
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
  && apt-get -y install \
    build-essential \
    cmake \
    g++-multilib \
    gettext \
    git \
    lib32ncurses5-dev \
    locales \
    ninja-build \
    pkg-config \
    python3 \
    python3-pexpect \
    sudo \
  && locale-gen en_US.UTF-8 \
  && apt-get clean

RUN groupadd -g 1000 ghotiuser \
  && useradd -p $(openssl passwd -1 ghoti) -d /home/ghotiuser -m -u 1000 -g 1000 ghotiuser \
  && adduser ghotiuser sudo \
  && mkdir -p /home/ghotiuser/ghoti-build \
  && mkdir /ghoti-source \
  && chown -R ghotiuser:ghotiuser /home/ghotiuser /ghoti-source

USER ghotiuser
WORKDIR /home/ghotiuser

COPY ghoti_run_tests.sh /

CMD /ghoti_run_tests.sh
