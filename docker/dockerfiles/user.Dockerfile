#
# Copyright 2020, Data61/CSIRO
#
# SPDX-License-Identifier: BSD-2-Clause
#

ARG EXTRAS_IMG=extras
# hadolint ignore=DL3006
FROM $EXTRAS_IMG

# Get user UID and username
ARG UID
ARG UNAME
ARG GID
ARG GROUP

COPY utils/user.sh /tmp/

RUN /bin/bash /tmp/user.sh

ENV TERM=xterm-256color

VOLUME /home/${UNAME}
VOLUME /isabelle
