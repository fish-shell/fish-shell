# SPDX-FileCopyrightText: © 2020 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

# RUN: %fish %s
argparse r-require= -- --require 2>/dev/null
echo $status
# CHECK: 2
argparse r-require= -- --require 2>&-
echo $status
# CHECK: 2
