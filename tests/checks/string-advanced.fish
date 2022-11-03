# SPDX-FileCopyrightText: © 2021 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish --features regex-easyesc %s
string replace -r 'a(.*)' '\U$0\E' abc
# CHECK: ABC
