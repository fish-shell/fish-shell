FROM centos:latest

# Build dependency
RUN yum update -y &&\
  yum install -y clang cmake gcc-c++ make ncurses-devel &&\
  yum clean all

# Test dependency
RUN yum install -y expect vim-common

ADD . /src
WORKDIR /src

# Build fish
RUN cmake . &&\
  make &&\
  make install

