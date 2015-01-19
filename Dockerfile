FROM centos:latest

# Build dependency
RUN yum update -y &&\
  yum install -y autoconf automake bc clang gcc-c++ make ncurses-devel &&\
  yum clean all

# Test dependency
RUN yum install -y expect vim-common

ADD . /src
WORKDIR /src

# Build fish
RUN autoreconf &&\
  ./configure &&\
  make &&\
  make install

