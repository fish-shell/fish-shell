FROM fedora:latest
LABEL org.opencontainers.image.source=https://github.com/ghoti-shell/ghoti-shell

RUN dnf install --assumeyes \
      cmake \
      diffutils \
      gcc-c++ \
      git-core \
      ncurses-devel \
      ninja-build \
      pcre2-devel \
      python3 \
      python3-pip \
      openssl \
      procps \
      sudo && \
    dnf clean all

RUN pip3 install pexpect

RUN groupadd -g 1000 ghotiuser \
  && useradd  -p $(openssl passwd -1 ghoti) -d /home/ghotiuser -m -u 1000 -g 1000 ghotiuser -G wheel \
  && mkdir -p /home/ghotiuser/ghoti-build \
  && mkdir /ghoti-source \
  && chown -R ghotiuser:ghotiuser /home/ghotiuser /ghoti-source

USER ghotiuser
WORKDIR /home/ghotiuser

COPY ghoti_run_tests.sh /

CMD /ghoti_run_tests.sh
