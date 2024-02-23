FROM fedora:latest
LABEL org.opencontainers.image.source=https://github.com/fish-shell/fish-shell

RUN dnf install --assumeyes \
      cmake \
      diffutils \
      gcc-c++ \
      git-core \
      ninja-build \
      pcre2-devel \
      python3 \
      python3-pip \
      openssl \
      procps \
      sudo && \
    dnf clean all

RUN pip3 install pexpect

RUN groupadd -g 1000 fishuser \
  && useradd  -p $(openssl passwd -1 fish) -d /home/fishuser -m -u 1000 -g 1000 fishuser -G wheel \
  && mkdir -p /home/fishuser/fish-build \
  && mkdir /fish-source \
  && chown -R fishuser:fishuser /home/fishuser /fish-source

USER fishuser
WORKDIR /home/fishuser

COPY fish_run_tests.sh /

CMD /fish_run_tests.sh
