FROM centos:8
LABEL org.opencontainers.image.source=https://github.com/fish-shell/fish-shell

# See https://stackoverflow.com/questions/70963985/error-failed-to-download-metadata-for-repo-appstream-cannot-prepare-internal

RUN cd /etc/yum.repos.d/ && \
  sed -i 's/mirrorlist/#mirrorlist/g' /etc/yum.repos.d/CentOS-* && \
  sed -i 's|#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-*


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
    pcre2-devel \
    sudo \
  && yum clean all

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
