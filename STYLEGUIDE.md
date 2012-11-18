# Style guide

This is style guide for fish contributors. You should use it for any new code
that you would add to this project and try to format existing code to use this
style.

## Formatting

1. Always use 2 spaces instead tabs as indent (unless needed like `Makefile`).
2. Opening curly bracket is always the same line as declaration.

        // ✔:
        struct name {
          // code
        };

        void func() {
          // code
        }

        if (...) {
          // code
        }

        // ✗:
        void func()
        {
          // code
        }

3. Put space after `if`, `while` and `for` before conditions.

        // ✔:
        if () {}

        // ✗:
        if() {}

4. Put spaces before and after operators excluding increment and decrement;

        // ✔:
        int a = 1 + 2 * 3;
        a++;

        // ✗:
        int a=1+2*3;
        a ++;

5. Never put spaces between function name and parameters list.

        // ✔:
        func(args);

        // ✗:
        func (args);

6. Never put spaces after `(` and before `)`.
7. Always put space after comma and semicolon.

        // ✔:
        func(arg1, arg2);

        for (int i = 0; i < LENGTH; i++) {}

        // ✗:
        func(arg1,arg2);

        for (int i = 0;i<LENGTH;i++) {}

## Documentation

Document your code using [Doxygen][dox].

1. Documentation comment should use double star notation or tipple slash:

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

All names in code should be `small_snake_case`. No Hungary notation is used.
Classes and structs names should be followed by `_t`.

[dox]: http://www.stack.nl/~dimitri/doxygen/ "Doxygen homepage"
