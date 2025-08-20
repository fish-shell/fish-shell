# fish-printf

The printf implementation used in [fish-shell](https://fishshell.com), based on musl printf.

[![crates.io](https://img.shields.io/crates/v/fish-printf.svg)](https://crates.io/crates/fish-printf)

Licensed under the MIT license.

### Usage

Run `cargo add fish-printf` to add this crate to your `Cargo.toml` file.

### Notes

fish-printf attempts to match the C standard for printf. It supports the following features:

-   Locale-specific formatting (decimal point, thousands separator, etc.)
-   Honors the current rounding mode.
-   Supports the `%n` modifier for counting characters written.

fish-printf does not support positional arguments, such as `printf("%2$d", 1, 2)`.

Prefixes like `l` or `ll` are recognized, but only used for validating the format string.
The size of integer values is taken from the argument type.

fish-printf can output to an `std::fmt::Write` object, or return a string.

For reasons related to fish-shell, fish-printf has a feature "widestring" which uses the [widestring](https://crates.io/crates/widestring) crate. This is off by default. If enabled, run `cargo add widestring` to add the widestring crate.

### Examples

```rust
use fish_printf::sprintf;

// Create a `String` from a format string.
let s = sprintf!("%0.5g", 123456.0) // 1.2346e+05

// Append to an existing string.
let mut s = String::new();
sprintf!(=> &mut s, "%0.5g", 123456.0) // 1.2346e+05
```

See the crate documentation for additional examples.
