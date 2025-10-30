// Support for printf-style formatting.
#[macro_export]
macro_rules! sprintf {
    // Allow a `&str` or `&Utf32Str` as a format, and return a `Utf32String`.
    ($fmt:expr $(, $arg:expr)* $(,)?) => {
        {
            let mut target = widestring::Utf32String::new();
            $crate::sprintf!(=> &mut target, $fmt, $($arg),*);
            target
        }
    };

    // Allow a `&str` or `&Utf32Str` as a format, and write to a target,
    // which should be a `&mut String` or `&mut Utf32String`.
    //
    (=> $target:expr, $fmt:expr $(, $arg:expr)* $(,)?) => {
        {
            let _ = fish_printf::sprintf!(=> $target, $fmt, $($arg),*);
        }
    };
}

#[macro_export]
macro_rules! fprintf {
    // Allow a `&str` or `&Utf32Str` as a format, and write to an fd.
    ($fd:expr, $fmt:expr $(, $arg:expr)* $(,)?) => {
        {
            let wide = $crate::wutil::sprintf!($fmt, $( $arg ),*);
            $crate::wutil::unescape_bytes_and_write_to_fd(&wide, $fd);
        }
    }
}

#[macro_export]
macro_rules! printf {
    // Allow a `&str` or `&Utf32Str` as a format, and write to stdout.
    ($fmt:expr $(, $arg:expr)* $(,)?) => {
        $crate::fprintf!(libc::STDOUT_FILENO, $fmt $(, $arg)*)
    }
}

#[macro_export]
macro_rules! eprintf {
    // Allow a `&str` or `&Utf32Str` as a format, and write to stderr.
    ($fmt:expr $(, $arg:expr)* $(,)?) => {
        fprintf!(libc::STDERR_FILENO, $fmt $(, $arg)*)
    }
}

pub use {eprintf, fprintf, printf, sprintf};

#[cfg(test)]
mod tests {
    // Test basic sprintf with both literals and wide strings.
    #[test]
    fn test_sprintf() {
        assert_eq!(sprintf!("Hello, %s!", "world"), "Hello, world!");
    }
}
