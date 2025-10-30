#[macro_export]
macro_rules! write_newline_to_fd {
    ($fd:expr) => {{
        let _ = $crate::wutil::write_to_fd(b"\n", $fd);
    }};
}
pub use write_newline_to_fd;

#[macro_export]
macro_rules! fprint {
    ($fd:expr, $output:expr) => {{
        let _ = $crate::wutil::unescape_and_write_to_fd($output, $fd);
    }};
}
pub use fprint;

#[macro_export]
macro_rules! localized_fprint {
    ($fd:expr, $id:expr) => {
        $crate::localization::fprint!($fd, fish_fluent::localize!($id))
    };
    ($fd:expr, $id:expr, $args:expr) => {
        $crate::localization::fprint!($fd, fish_fluent::localize!($id, $args))
    };
    ($fd:expr, $id:expr, $($key:literal: $value:expr),+ $(,)?) => {
        $crate::localization::fprint!($fd, fish_fluent::localize!($id, $($key: $value),*))
    };
}
pub use localized_fprint;

#[macro_export]
macro_rules! localized_fprintln {
    ($fd:expr, $id:expr) => {{
        $crate::localization::localized_fprint!($fd, $id);
        $crate::localization::write_newline_to_fd!($fd);
    }};
    ($fd:expr, $id:expr, $args:expr) => {{
        $crate::localization::localized_fprint!($fd, $id, $args);
        $crate::localization::write_newline_to_fd!($fd);
    }};
    ($fd:expr, $id:expr, $($key:literal: $value:expr),+ $(,)?) => {{
        $crate::localization::localized_fprint!($fd, $id, $($key: $value),*);
        $crate::localization::write_newline_to_fd!($fd);
    }};
}
pub use localized_fprintln;

#[macro_export]
macro_rules! localized_print {
    ($id:expr) => {
        $crate::localization::localized_fprint!(libc::STDOUT_FILENO, $id)
    };
    ($id:expr, $args:expr) => {
        $crate::localization::localized_fprint!(libc::STDOUT_FILENO, $id, $args)
    };
    ($id:expr, $($key:literal: $value:expr),+ $(,)?) => {
        $crate::localization::localized_fprint!(libc::STDOUT_FILENO, $id, $($key: $value),*)
    };
}
pub use localized_print;

#[macro_export]
macro_rules! localized_println {
    ($id:expr) => {{
        let fd = libc::STDOUT_FILENO;
        $crate::localization::localized_fprint!(fd, $id);
        $crate::localization::write_newline_to_fd!(fd);
    }};
    ($id:expr, $args:expr) => {{
        let fd = libc::STDOUT_FILENO;
        $crate::localization::localized_fprint!(fd, $id, $args);
        $crate::localization::write_newline_to_fd!(fd);
    }};
    ($id:expr, $($key:literal: $value:expr),+ $(,)?) => {{
        let fd = libc::STDOUT_FILENO;
        $crate::localization::localized_fprint!(fd, $id, $($key: $value),*);
        $crate::localization::write_newline_to_fd!(fd);
    }};
}
pub use localized_println;

#[macro_export]
macro_rules! localized_eprint {
    ($id:expr) => {
        $crate::localization::localized_fprint!(libc::STDERR_FILENO, $id)
    };
    ($id:expr, $args:expr) => {
        $crate::localization::localized_fprint!(libc::STDERR_FILENO, $id, $args)
    };
    ($id:expr, $($key:literal: $value:expr),+ $(,)?) => {
        $crate::localization::localized_fprint!(libc::STDERR_FILENO, $id, $($key: $value),*)
    };
}
pub use localized_eprint;

#[macro_export]
macro_rules! localized_eprintln {
    ($id:expr) => {{
        let fd = libc::STDERR_FILENO;
        $crate::localization::localized_fprint!(fd, $id);
        $crate::localization::write_newline_to_fd!(fd);
    }};
    ($id:expr, $args:expr) => {{
        let fd = libc::STDERR_FILENO;
        $crate::localization::localized_fprint!(fd, $id, $args);
        $crate::localization::write_newline_to_fd!(fd);
    }};
    ($id:expr, $($key:literal: $value:expr),+ $(,)?) => {{
        let fd = libc::STDERR_FILENO;
        $crate::localization::localized_fprint!(fd, $id, $($key: $value),*);
        $crate::localization::write_newline_to_fd!(fd);
    }};
}
pub use localized_eprintln;

#[macro_export]
macro_rules! localized_sprint {
    ($id:expr) => {
        fish_fluent::localize!($id)
    };
    ($id:expr, $args:expr) => {
        fish_fluent::localize!($id, $args)
    };
    ($id:expr, $($key:literal: $value:expr),+ $(,)?) => {
        fish_fluent::localize!($id, $($key: $value),*)
    };
}
pub use localized_sprint;

#[macro_export]
macro_rules! sprintln {
    ($input:expr) => {{
        let mut res = $input.into_owned();
        res.push('\n');
        res
    }};
}
pub use sprintln;

#[macro_export]
macro_rules! localized_sprintln {
    ($id:expr) => {
        $crate::localization::sprintln!(fish_fluent::localize!($id))
    };
    ($id:expr, $args:expr) => {
        $crate::localization::sprintln!(fish_fluent::localize!($id, $args))
    };
    ($id:expr, $($key:literal: $value:expr),+ $(,)?) => {
        $crate::localization::sprintln!(fish_fluent::localize!($id, $($key: $value),*))
    };
}
pub use localized_sprintln;

#[cfg(test)]
mod tests {
    use fish_fluent::fluent_id;

    #[test]
    fn without_args() {
        let id = fluent_id!("test");
        localized_print!(id);
        localized_println!(id);
        localized_eprint!(id);
        localized_eprintln!(id);
        localized_fprint!(libc::STDOUT_FILENO, id);
        localized_fprintln!(libc::STDOUT_FILENO, id);
        assert_eq!(localized_sprint!(id), "This is a test");
        assert_eq!(localized_sprintln!(id), "This is a test\n");
    }

    #[test]
    fn with_args() {
        let id = fluent_id!("test-with-args");
        localized_print!(id, "first": 1, "second": "two");
        localized_println!(id, "first": 1, "second": "two");
        localized_eprint!(id, "first": 1, "second": "two");
        localized_eprintln!(id, "first": 1, "second": "two");
        localized_fprint!(libc::STDOUT_FILENO, id, "first": 1, "second": "two");
        localized_fprintln!(libc::STDOUT_FILENO, id, "first": 1, "second": "two");
        assert_eq!(
            localized_sprint!(id, "first": 1, "second": "two"),
            "Two arguments: \u{2068}1\u{2069}, \u{2068}two\u{2069}"
        );
        assert_eq!(
            localized_sprintln!(id, "first": 1, "second": "two"),
            "Two arguments: \u{2068}1\u{2069}, \u{2068}two\u{2069}\n"
        );

        let mut args = fluent::FluentArgs::new();
        args.set("first", 1);
        args.set("second", "two");

        localized_print!(id, &args);
        localized_println!(id, &args);
        localized_eprint!(id, &args);
        localized_eprintln!(id, &args);
        localized_fprint!(libc::STDOUT_FILENO, id, &args);
        localized_fprintln!(libc::STDOUT_FILENO, id, &args);
        assert_eq!(
            localized_sprint!(id, &args),
            "Two arguments: \u{2068}1\u{2069}, \u{2068}two\u{2069}"
        );
        assert_eq!(
            localized_sprintln!(id, &args),
            "Two arguments: \u{2068}1\u{2069}, \u{2068}two\u{2069}\n"
        );
    }
}
