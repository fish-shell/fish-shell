# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish %s
begin
end >.
status -b; and echo "status -b returned true after bad redirect on a begin block"
# Note that we sometimes get fancy quotation marks here, so let's match three characters
#CHECKERR: warning: An error occurred while redirecting file {{...}}
#CHECKERR: {{open: Is a directory|open: Invalid argument}}
echo $status
#CHECK: 1
