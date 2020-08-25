FROM centos:latest

# Build dependency
RUN dnf update -y &&\
    dnf install -y epel-release &&\
    dnf install -y clang cmake3 gcc-c++ make ncurses-devel &&\
    dnf clean all

# Test dependency
RUN dnf install -y expect vim-common

ADD . /src
WORKDIR /src

# Build fish
RUN cmake3 . &&\
  make &&\
  make install

