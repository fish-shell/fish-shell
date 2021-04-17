FROM fedora:latest

RUN dnf install --assumeyes \
    cmake \
    gcc-c++ \
    git-core \
    ncurses-devel \
    ninja-build \
    python3 \
    python3-pip \
    openssl \
    sudo

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
