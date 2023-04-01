#!/usr/bin/env python3

# Finds potential ODR violations due to weak symbols.
# For example, if you have two different structs with the same name in different files,
# their inline constructors may collide.
# This works only on Linux. It is designed to be run from the cmake build directory.
# clang seems more willing to emit non-inlined ctors. Of course perform a Debug build.

import re
import subprocess

output = subprocess.check_output(
    "nm --radix=d -g --demangle -l --print-size CMakeFiles/ghotilib.dir/src/*.o",
    shell=True,
    universal_newlines=True,
)
files_by_name = {}  # Symbol to set of paths
sizes_by_name = {}  # Symbol to set of int sizes

for line in output.split("\n"):
    # Keep only weak symbols with default values (e.g. emitted inline functions).
    # Example line: "0000000000000000 0000000000000107 W symbol_name"
    # First number is offset, second is size.
    # Note this is decimal because of radix=d.
    m = re.match(r"\d+ (\d+) W (.*)\t(.*)", line)
    if not m:
        continue
    size, name, filename = m.groups()
    files_by_name.setdefault(name, set()).add(filename)
    sizes_by_name.setdefault(name, set()).add(int(size))

odr_violations = 0
for name, sizes in sizes_by_name.items():
    if len(sizes) == 1:
        continue
    files = files_by_name[name]
    # Ignore symbols that only appear in one file.
    # These are typically headers - unclear why they get different sizes but it appears benign.
    if len(files) == 1:
        continue
    # Multiple sizes for this symbol name.
    odr_violations += 1
    print("Multiple sizes for symbol: " + name)
    print("\t%s" % ", ".join([str(x) for x in sizes]))
    print("\tFound in files:")
    for filename in files:
        print("\t\t%s" % filename)

if odr_violations == 0:
    print("No ODR violations found, hooray\n")

# Show potential weak symbols.
suspicious_odrs = 0
for (name, files) in files_by_name.items():
    if len(files) != 1:
        continue
    (filename,) = files
    if ".cpp" in filename:
        if suspicious_odrs == 0:
            print("Some suspicious singles:")
        suspicious_odrs += 1
        print("\t%s" % name)
        print("\t\tIn file %s" % filename)
