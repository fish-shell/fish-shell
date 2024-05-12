/** Rust printf implementation, based on musl. */
mod arg;
pub use arg::{Arg, ToArg};

mod fmt_fp;
mod printf_impl;
pub use printf_impl::{sprintf_locale, Error};
pub mod locale;
pub use locale::{Locale, C_LOCALE, EN_US_LOCALE};

#[cfg(test)]
mod tests;

#[macro_export]
macro_rules! sprintf {
    // Variant which allows a string literal and returns a `Utf32String`.
    ($fmt:literal, $($arg:expr),* $(,)?) => {
        {
            let mut target = widestring::Utf32String::new();
            $crate::sprintf!(=> &mut target, widestring::utf32str!($fmt), $($arg),*);
            target
        }
    };

    // Variant which allows a string literal and writes to a target.
    // The target should implement std::fmt::Write.
    (
        => $target:expr, // target string
        $fmt:literal, // format string
        $($arg:expr),* // arguments
        $(,)? // optional trailing comma
    ) => {
        {
            $crate::sprintf!(=> $target, widestring::utf32str!($fmt), $($arg),*);
        }
    };

    // Variant which allows a `Utf32String` as a format, and writes to a target.
    (
        => $target:expr, // target string
        $fmt:expr, // format string as UTF32String
        $($arg:expr),* // arguments
        $(,)? // optional trailing comma
    ) => {
        {
            // May be no args!
            #[allow(unused_imports)]
            use $crate::ToArg;
            $crate::sprintf_c_locale(
                $target,
                $fmt.as_char_slice(),
                &mut [$($arg.to_arg()),*],
            ).unwrap()
        }
    };

    // Variant which allows a `Utf32String` as a format, and returns a `Utf32String`.
    ($fmt:expr, $($arg:expr),* $(,)?) => {
        {
            let mut target = widestring::Utf32String::new();
            $crate::sprintf!(=> &mut target, $fmt, $($arg),*);
            target
        }
    };
}

/// Formats a string using the provided format specifiers and arguments, using the C locale,
/// and writes the output to the given `Write` implementation.
///
/// # Parameters
/// - `f`: The receiver of formatted output.
/// - `fmt`: The format string being parsed.
/// - `locale`: The locale to use for number formatting.
/// - `args`: Iterator over the arguments to format.
///
/// # Returns
/// A `Result` which is `Ok` containing the number of bytes written on success, or an `Error`.
pub fn sprintf_c_locale(
    f: &mut impl std::fmt::Write,
    fmt: &[char],
    args: &mut [Arg],
) -> Result<usize, Error> {
    sprintf_locale(f, fmt, &locale::C_LOCALE, args)
}
