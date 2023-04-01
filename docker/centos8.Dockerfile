FROM centos:8
LABEL org.opencontainers.image.source=https://github.com/ghoti-shell/ghoti-shell

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

RUN groupadd -g 1000 ghotiuser \
  && useradd  -p $(openssl passwd -1 ghoti) -d /home/ghotiuser -m -u 1000 -g 1000 ghotiuser -G wheel \
  && mkdir -p /home/ghotiuser/ghoti-build \
  && mkdir /ghoti-source \
  && chown -R ghotiuser:ghotiuser /home/ghotiuser /ghoti-source

USER ghotiuser
WORKDIR /home/ghotiuser

COPY ghoti_run_tests.sh /

CMD /ghoti_run_tests.sh
