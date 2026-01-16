/** Rust printf implementation, based on musl. */
mod arg;
pub use arg::{Arg, ToArg};

mod fmt_fp;
mod printf_impl;
pub use printf_impl::{Error, FormatString, sprintf_locale};
pub mod locale;

#[cfg(test)]
mod tests;

/// A macro to format a string using `fish_printf` with C-locale formatting rules.
///
/// # Examples
///
/// ```
/// use fish_printf::sprintf;
///
/// // Create a `String` from a format string.
/// let s = sprintf!("%0.5g", 123456.0);
/// assert_eq!(s, "1.2346e+05");
///
/// // Append to an existing string.
/// let mut s = String::new();
/// sprintf!(=> &mut s, "%0.5g", 123456.0);
/// assert_eq!(s, "1.2346e+05");
/// ```
#[macro_export]
macro_rules! sprintf {
    // Write to a newly allocated String, and return it.
    // This panics if the format string or arguments are invalid.
    (
        $fmt:expr // Format string, which should implement FormatString.
        $(, $($arg:expr),*)? // arguments
    ) => {
        {
            let mut target = String::new();
            $crate::sprintf!(=> &mut target, $fmt $(, $($arg),*)?);
            target
        }
    };

    // Variant which writes to a target.
    // The target should implement std::fmt::Write.
    (
        => $target:expr, // target string
        $fmt:expr // format string
        $(, $($arg:expr),*)? // arguments
    ) => {
        {
            // May be no args!
            #[allow(unused_imports)]
            use $crate::ToArg;
            $crate::printf_c_locale(
                $target,
                $fmt,
                &mut [$( $($arg.to_arg()),* )?],
            ).unwrap()
        }
    };
}

/// Formats a string using the provided format specifiers and arguments, using the C locale,
/// and writes the output to the given `Write` implementation.
///
/// # Parameters
/// - `f`: The receiver of formatted output.
/// - `fmt`: The format string being parsed.
/// - `args`: Iterator over the arguments to format.
///
/// # Returns
/// A `Result` which is `Ok` containing the width of the string written on success, or an `Error`.
///
/// # Example
///
/// ```
/// use fish_printf::{printf_c_locale, ToArg, FormatString};
/// use std::fmt::Write;
///
/// let mut output = String::new();
/// let fmt: &str = "%0.5g";  // Example format string
/// let mut args = [123456.0.to_arg()];
///
/// let result = printf_c_locale(&mut output, fmt, &mut args);
///
/// assert_eq!(result, Ok(10));
/// assert_eq!(output, "1.2346e+05");
/// ```
pub fn printf_c_locale(
    f: &mut impl std::fmt::Write,
    fmt: impl FormatString,
    args: &mut [Arg],
) -> Result<usize, Error> {
    sprintf_locale(f, fmt, &locale::C_LOCALE, args)
}
