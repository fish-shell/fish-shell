FROM alpine:3.13
LABEL org.opencontainers.image.source=https://github.com/fish-shell/fish-shell

ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8

RUN apk add --no-cache \
  bash \
  cmake \
  g++ \
  gettext-dev \
  git \
  libintl \
  musl-dev \
  ncurses-dev \
  ninja \
  pcre2-dev \
  python3 \
  py3-pexpect

RUN addgroup -g 1000 fishuser

RUN adduser \
    --disabled-password \
    --gecos "" \
    --home "/home/fishuser" \
    --ingroup fishuser \
    --uid 1000 \
    fishuser

RUN mkdir -p /home/fishuser/fish-build \
  && mkdir /fish-source \
  && chown -R fishuser:fishuser /home/fishuser /fish-source

USER fishuser
WORKDIR /home/fishuser

COPY fish_run_tests.sh /

CMD /fish_run_tests.sh
