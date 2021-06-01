FROM centos:8

# install powertools to get ninja-build
RUN dnf -y install dnf-plugins-core \
  && dnf config-manager --set-enabled powertools \
  && yum install --assumeyes epel-release \
  && yum install --assumeyes \
    cmake \
    diffutils \
    gcc-c++ \
    git \
    ncurses-devel \
    ninja-build \
    python3 \
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
