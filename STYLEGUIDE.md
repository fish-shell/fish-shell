# Style guide

This is style guide for fish contributors. You should use it for any new code
that you would add to this project and try to format existing code to use this
style.

## Formatting

1. fish uses the Allman/BSD style of indentation.
2. Indent with spaces, not tabs.
3. Use 4 spaces per indent (unless needed like `Makefile`).
4. Opening curly bracket is on the following line:

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

5. Put space after `if`, `while` and `for` before conditions.

        // ✔:
        if () {}

        // ✗:
        if() {}

6. Put spaces before and after operators excluding increment and decrement;

        // ✔:
        int a = 1 + 2 * 3;
        a++;

        // ✗:
        int a=1+2*3;
        a ++;

7. Never put spaces between function name and parameters list.

        // ✔:
        func(args);

        // ✗:
        func (args);

8. Never put spaces after `(` and before `)`.
9. Always put space after comma and semicolon.

        // ✔:
        func(arg1, arg2);

        for (int i = 0; i < LENGTH; i++) {}

        // ✗:
        func(arg1,arg2);

        for (int i = 0;i<LENGTH;i++) {}

## Documentation

Document your code using [Doxygen][dox].

1. Documentation comment should use double star notation or tripple slash:

        // ✔:
        /// Some var
        int var;

        /**
         * Some func
         */
        void func();

2. Use slash as tag mark:

        // ✔:

        /**
         * \param a an integer argument.
         * \param s a constant character pointer.
         * \return The results
         */
        int foo(int a, const char *s);

## Naming

All names in code should be `small_snake_case`. No Hungarian notation is used.
Classes and structs names should be followed by `_t`.

[dox]: http://www.stack.nl/~dimitri/doxygen/ "Doxygen homepage"
