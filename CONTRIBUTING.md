# Guidelines For Developers

This document provides guidelines for making changes to the fish-shell project. This includes rules for how to format the code, naming conventions, etc. It also includes recommended best practices such as creating a Travis-CI account so you can verify your changes pass all the tests before making a pull-request.

See the bottom of this document for help on installing the linting and style reformatting tools discussed in the following sections.

## Lint Free Code

Automated analysis tools like cppcheck and oclint can point out potential bugs. They also help ensure the code has a consistent style and that it avoids patterns that tend to confuse people.

Ultimately we want lint free code. However, at the moment a lot of cleanup is required to reach that goal. For now simply try to avoid introducing new lint.

To make linting the code easy there are two make targets: `lint` and `lint-all`. The latter does just what the name implies. The former will lint any modified but not committed `*.cpp` files. If there is no uncommitted work it will lint the files in the most recent commit.

## Ensuring Your Changes Conform to the Style Guides

The following sections discuss the specific rules for the style that should be used when writing fish code. To ensure your changes conform to the style rules you simply need to run

```
make style
```

before commiting your change. If you've already committed your changes that's okay since it will then check the files in the most recent commit. This can be useful after you've merged someone elses change and want to check that it's style is acceptable.

If you want to check the style of the entire code base run

```
make style-all
```

## Fish Script Style Guide

Fish scripts such as those in the *share/functions* and *tests* directories should be formatted using the `fish_indent` command.

Function names should be all lowercase with undescores separating words. Private functions should begin with an underscore. The first word should be `fish` if the function is unique to fish.

The first word of global variable names should generally be `fish` for public vars or `_fish` for private vars to minimize the possibility of name clashes with user defined vars.

## C++ Style Guide

1. The [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) forms the basis of the fish C++ style guide. There are two major deviations for the fish project. First, a four, rather than two, space indent. Second, line lengths up to 100, rather than 80, characters.

1. The `clang-format` command is authoritative with respect to indentation, whitespace around operators, etc. **Note**: this rule should be ignored at this time. After the code is cleaned up this rule will become mandatory.

1. All names in code should be `small_snake_case`. No Hungarian notation is used. Classes and structs names should be followed by `_t`.

1. Always attach braces to the surrounding context.

1. Indent with spaces, not tabs and use four spaces per indent.

## Installing the Required Tools

### Installing the Linting Tools

To install the lint checkers on Mac OS X using HomeBrew:

```
brew tap oclint/formulae
brew install oclint
brew install cppcheck
```

To install the lint checkers on Linux distros that use Apt:

```
sudo apt-get install clang
sudo apt-get install oclint
sudo apt-get install cppcheck
```

### Installing the Reformatting Tools

To install the reformatting tool on Mac OS X using HomeBrew:

```
brew install clang-format
```

To install the reformatting tool on Linux distros that use Apt:

```
apt-cache search clang-format
```

That will list the versions available. Pick the newest one available (3.6 for Ubuntu 14.04 as I write this) and install it:

```
sudo apt-get install clang-format-3.6
sudo ln -s /usr/bin/clang-format-3.6 /usr/bin/clang-format

```
