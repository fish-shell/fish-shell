FROM alpine:3.13
LABEL org.opencontainers.image.source=https://github.com/ghoti-shell/ghoti-shell

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

RUN addgroup -g 1000 ghotiuser

RUN adduser \
    --disabled-password \
    --gecos "" \
    --home "/home/ghotiuser" \
    --ingroup ghotiuser \
    --uid 1000 \
    ghotiuser

RUN mkdir -p /home/ghotiuser/ghoti-build \
  && mkdir /ghoti-source \
  && chown -R ghotiuser:ghotiuser /home/ghotiuser /ghoti-source

USER ghotiuser
WORKDIR /home/ghotiuser

COPY ghoti_run_tests.sh /

CMD /ghoti_run_tests.sh
