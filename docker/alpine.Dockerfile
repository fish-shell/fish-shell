FROM alpine:3.19
LABEL org.opencontainers.image.source=https://github.com/fish-shell/fish-shell

ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV PIP_ROOT_USER_ACTION=ignore

RUN apk add --no-cache \
    bash \
    cargo \
    g++ \
    gettext-dev \
    git \
    libintl \
    musl-dev \
    pcre2-dev \
    py3-pexpect \
    py3-pip \
    python3 \
    rust \
    rustfmt \
    sudo \
    tmux

RUN pip install --break-system-packages black

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

ENV FISH_CHECK_LINT=false

CMD /fish_run_tests.sh
