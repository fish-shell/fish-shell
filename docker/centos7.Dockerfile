FROM centos:7
LABEL org.opencontainers.image.source=https://github.com/ghoti-shell/ghoti-shell

# install epel first to get cmake3
RUN yum install --assumeyes epel-release https://repo.ius.io/ius-release-el7.rpm \
  && yum install --assumeyes \
    cmake3 \
    gcc-c++ \
    git236 \
    ncurses-devel \
    ninja-build \
    python3 \
    openssl \
    pcre2-devel \
    sudo \
  && yum clean all

# cmake is called "cmake3" on centos7.
RUN ln -s /usr/bin/cmake3 /usr/bin/cmake \
  && pip3 install pexpect

RUN groupadd -g 1000 ghotiuser \
  && useradd  -p $(openssl passwd -1 ghoti) -d /home/ghotiuser -m -u 1000 -g 1000 ghotiuser -G wheel \
  && mkdir -p /home/ghotiuser/ghoti-build \
  && mkdir /ghoti-source \
  && chown -R ghotiuser:ghotiuser /home/ghotiuser /ghoti-source

USER ghotiuser
WORKDIR /home/ghotiuser

COPY ghoti_run_tests.sh /

CMD /ghoti_run_tests.sh
