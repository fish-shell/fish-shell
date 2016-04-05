# Guidelines For Developers

## Lint Free Code

Automated analysis tools like cppcheck and oclint can help identify bugs. They also help ensure the code has a consistent style and avoids patterns that tend to confuse people.

Ultimately we want lint free code. However, at the moment a lot of cleanup is required to reach that goal. For now simply try to avoid introducing new lint.

To make linting the code easy there are two make targets: `lint` and `lint-all`. The latter does just what the name implies. The former will lint any modified but not committed `*.cpp` files. If there is no uncommitted work it will lint the files in the most recent commit.

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

## Fish Script Style Guide

Fish scripts such as those in the *share/functions* and *tests* directories should be formatted using the `fish_indent` command.

Function names should be all lowercase with undescores separating words. Private functions should begin with an underscore. The first word should be `fish` if the function is unique to fish.

The first word of global variable names should generally be `fish` for public vars or `_fish` for private vars to minimize the possibility of name clashes with user defined vars.

## C++ Style Guide

1. The `clang-format` command is authoritative with respect to indentation, whitespace around operators, etc. **Note**: this rule should be ignored at this time. A subsequent commit will add the necessary config file and make targets. After the happens the code will be cleaned up and this rule will become mandatory.

1. All names in code should be `small_snake_case`. No Hungarian notation is used. Classes and structs names should be followed by `_t`.

1. fish uses the Allman/BSD style of indentation.

1. Indent with spaces, not tabs.

1. Use 4 spaces per indent.

1. Opening curly bracket is on the following line:

        // ✔:
        struct name
        {
          // code
        };

        void func()
        {
          // code
        }

        if (...)
        {
          // code
        }

        // ✗:
        void func() {
          // code
        }

1. Put space after `if`, `while` and `for` before conditions.

        // ✔:
        if () {}

        // ✗:
        if() {}

1. Put spaces before and after operators excluding increment and decrement;

        // ✔:
        int a = 1 + 2 * 3;
        a++;

        // ✗:
        int a=1+2*3;
        a ++;

1. Never put spaces between function name and parameters list.

        // ✔:
        func(args);

        // ✗:
        func (args);

1. Never put spaces after `(` and before `)`.

1. Always put space after comma and semicolon.

        // ✔:
        func(arg1, arg2);

        for (int i = 0; i < LENGTH; i++) {}

        // ✗:
        func(arg1,arg2);

        for (int i = 0;i<LENGTH;i++) {}
