# SPDX-FileCopyrightText: © 2020 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

# RUN: %fish %s

set -l oldpwd $PWD
cd (mktemp -d)
set tmpdir (pwd -P)

# Hidden files are only matched with explicit dot.
touch .hidden visible
string join \n * | sort
# CHECK: visible
string join \n .* | sort
# CHECK: .hidden
rm -Rf .hidden visible

# Trailing slash matches only directories.
touch abc1
mkdir abc2
string join \n * | sort
# CHECK: abc1
# CHECK: abc2
string join \n */ | sort
# CHECK: abc2/
rm -Rf *

# Symlinks are descended into independently.
# Here dir2/link2 is symlinked to dir1/child1.
# The contents of dir2 will be explored twice.
mkdir -p dir1/child1
touch dir1/child1/anyfile
mkdir dir2
ln -s ../dir1/child1 dir2/link2
string join \n **/anyfile | sort
# CHECK: dir1/child1/anyfile
# CHECK: dir2/link2/anyfile

# But symlink loops only get explored once.
mkdir -p dir1/child2/grandchild1
touch dir1/child2/grandchild1/differentfile
ln -s ../../child2/grandchild1 dir1/child2/grandchild1/link2
echo **/differentfile
# CHECK: dir1/child2/grandchild1/differentfile
rm -Rf *

# Recursive globs handling.
mkdir -p dir_a1/dir_a2/dir_a3
touch dir_a1/dir_a2/dir_a3/file_a
mkdir -p dir_b1/dir_b2/dir_b3
touch dir_b1/dir_b2/dir_b3/file_b
string join \n **/file_* | sort
# CHECK: dir_a1/dir_a2/dir_a3/file_a
# CHECK: dir_b1/dir_b2/dir_b3/file_b

string join \n **a3/file_* | sort
# CHECK: dir_a1/dir_a2/dir_a3/file_a

string join \n ** | sort
# CHECK: dir_a1
# CHECK: dir_a1/dir_a2
# CHECK: dir_a1/dir_a2/dir_a3
# CHECK: dir_a1/dir_a2/dir_a3/file_a
# CHECK: dir_b1
# CHECK: dir_b1/dir_b2
# CHECK: dir_b1/dir_b2/dir_b3
# CHECK: dir_b1/dir_b2/dir_b3/file_b

string join \n **/ | sort
# CHECK: dir_a1/
# CHECK: dir_a1/dir_a2/
# CHECK: dir_a1/dir_a2/dir_a3/
# CHECK: dir_b1/
# CHECK: dir_b1/dir_b2/
# CHECK: dir_b1/dir_b2/dir_b3/

string join \n **a2/** | sort
# CHECK: dir_a1/dir_a2/dir_a3
# CHECK: dir_a1/dir_a2/dir_a3/file_a

rm -Rf *

# Special behavior for #7222.
# The literal segment ** matches in the same directory.
mkdir foo
touch bar foo/bar
string join \n **/bar | sort
# CHECK: bar
# CHECK: foo/bar

# Clean up.
cd $oldpwd
rm -Rf $tmpdir
