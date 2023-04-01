FROM ubuntu:18.04
LABEL org.opencontainers.image.source=https://github.com/ghoti-shell/ghoti-shell

ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8

RUN apt-get update \
  && apt-get -y install \
    build-essential \
    cmake \
    clang-9 \
    gettext \
    git \
    libncurses5-dev \
    libpcre2-dev \
    locales \
    ninja-build \
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

ENV CXXFLAGS="-fno-omit-frame-pointer -fsanitize=undefined -fsanitize=address" \
  CC=clang-9 \
  CXX=clang++-9 \
  ASAN_OPTIONS=check_initialization_order=1:detect_stack_use_after_return=1:detect_leaks=1 \
  UBSAN_OPTIONS=print_stacktrace=1:report_error_type=1

CMD /ghoti_run_tests.sh
