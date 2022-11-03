# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

# RUN: %fish %s

# See issue 5692

function empty
end

# functions should not preserve $status
false
empty
echo $status
# CHECK: 0
true
empty
echo $status
# CHECK: 0

# blocks should preserve $status
false
begin
end
echo $status
# CHECK: 1
true
begin
end
echo $status
# CHECK: 0
