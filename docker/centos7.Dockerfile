FROM centos:7

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

RUN groupadd -g 1000 fishuser \
  && useradd  -p $(openssl passwd -1 fish) -d /home/fishuser -m -u 1000 -g 1000 fishuser -G wheel \
  && mkdir -p /home/fishuser/fish-build \
  && mkdir /fish-source \
  && chown -R fishuser:fishuser /home/fishuser /fish-source

USER fishuser
WORKDIR /home/fishuser

COPY fish_run_tests.sh /

CMD /fish_run_tests.sh
