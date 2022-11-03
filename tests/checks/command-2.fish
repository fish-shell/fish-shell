# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish -c "echo 1.2.3.4." -c "echo 5.6.7.8."
# CHECK: 1.2.3.4.
# CHECK: 5.6.7.8.
