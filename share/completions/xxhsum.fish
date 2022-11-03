# SPDX-FileCopyrightText: © 2020 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -c xxhsum -w xxh64sum
complete -c xxhsum -o H0 -d "32bits hash"
complete -c xxhsum -o H1 -d "64bits hash"
complete -c xxhsum -o H2 -d "128bits hash"
