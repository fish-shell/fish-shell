FROM tgagor/centos-stream:latest
LABEL org.opencontainers.image.source=https://github.com/fish-shell/fish-shell

# See https://stackoverflow.com/questions/70963985/error-failed-to-download-metadata-for-repo-appstream-cannot-prepare-internal

RUN cd /etc/yum.repos.d/ && \
  sed -i 's/mirrorlist/#mirrorlist/g' /etc/yum.repos.d/CentOS-* && \
  sed -i 's|#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-*


RUN dnf -y install dnf-plugins-core \
  && yum install --assumeyes epel-release \
  && yum install --assumeyes \
    cargo \
    diffutils \
    gcc-c++ \
    git \
    python3 \
    python3-pexpect \
    openssl \
    pcre2-devel \
    rustc \
    sudo \
    tmux \
  && yum clean all

RUN groupadd -g 1000 fishuser \
  && useradd  -p $(openssl passwd -1 fish) -d /home/fishuser -m -u 1000 -g 1000 fishuser -G wheel \
  && mkdir -p /home/fishuser/fish-build \
  && mkdir /fish-source \
  && chown -R fishuser:fishuser /home/fishuser /fish-source

USER fishuser
WORKDIR /home/fishuser

COPY fish_run_tests.sh /

ENV FISH_CHECK_LINT=false

CMD /fish_run_tests.sh
