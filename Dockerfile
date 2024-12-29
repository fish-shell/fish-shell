FROM centos:latest

# Build dependency
RUN yum update -y &&\
  yum install -y epel-release &&\
  yum install -y clang cmake3 gcc-c++ make &&\
  yum clean all

# Test dependency
RUN yum install -y expect vim-common

ADD . /src
WORKDIR /src

# Build fish
RUN cmake3 . &&\
  make &&\
  make install
