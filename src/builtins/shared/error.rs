use std::borrow::Cow;

use crate::{
    io::{IoStreams, OutputStream, StringOutputStream},
    parser::Parser,
    prelude::*,
    wgettext_fmt,
};

use fish_widestring::wstr;

#[macro_export]
macro_rules! err_fmt {
    (
        $string:expr // format string (literal or LocalizableString)
        $(, $args:expr)* // list of expressions
        $(,)?   // optional trailing comma
    ) => {
        $crate::builtins::error::Error::new(
            $crate::wgettext_fmt!($string, $($args),*).into()
        )
    };
}
pub use err_fmt;

#[macro_export]
macro_rules! err_str {
    (
        $string:expr // format string (literal or LocalizableString)
        $(,)?   // optional trailing comma
    ) => {
        $crate::builtins::error::Error::new(std::borrow::Cow::Borrowed($crate::wgettext!($string)))
    };
}
pub use err_str;

/// Generate an `Error` from a string without localization. This is typically
/// for error messages from external sources (e.g. C `strerror()`)
#[macro_export]
macro_rules! err_raw {
    (
        $string:expr // owned WString
    ) => {
        $crate::builtins::error::Error::new($string.into())
    };
}
pub use err_raw;

pub struct Error<'a> {
    msg: Cow<'a, wstr>,

    cmd: Option<&'a wstr>,
    subcmd: Option<&'a wstr>,
    parser: Option<&'a Parser>,
    hint: bool,
}

impl<'a> Error<'a> {
    localizable_consts!(
        /// Error message on missing argument.
        pub MISSING_OPT_ARG
        "%s: option requires an argument"

        /// Error message on unexpected argument.
        pub UNEXP_OPT_ARG
        "%s: option does not take an argument"

        /// Error message on missing man page.
        pub MISSING_HELP
        "missing man page\nDocumentation may not be installed.\n`help %s` will show an online version"

        /// Error message on multiple scope levels for variables.
        pub MULTIPLE_SCOPES
        "scope can be only one of: universal function global local"

        /// Error message for specifying both export and unexport to set/read.
        pub EXPORT_UNEXPORT
        "cannot both export and unexport"

        /// Error message for specifying both path and unpath to set/read.
        pub PATH_UNPATH
        "cannot both path and unpath"

        /// Error message for unknown switch.
        pub UNKNOWN_OPT
        "%s: unknown option"

        /// Error message for invalid bind mode name.
        pub BIND_MODE
        "%s: invalid mode name. See `help %s`"

        /// Error message when too many arguments are supplied to a builtin.
        pub TOO_MANY_ARGUMENTS
        "too many arguments"

        /// Error message when integer expected
        pub NOT_NUMBER
        "%s: invalid integer"

        /// Command that requires a subcommand was invoked without a recognized subcommand.
        pub MISSING_SUBCMD
        "missing subcommand"

        pub INVALID_SUBCMD
        "invalid subcommand"

        pub INVALID_SUBSUBCMD
        "%s: invalid subcommand"

        /// Error messages for unexpected args.
        pub MISSING_ARG
        "missing argument"

        pub UNEXP_ARG_COUNT
        "expected %d arguments; got %d"

        pub UNPEXP_ARG_COUNT_WITH_CTX
        "%s: expected %d arguments; got %d"

        pub MIN_ARG_COUNT
        "expected >= %d arguments; got %d"

        pub MAX_ARG_COUNT
        "expected <= %d arguments; got %d"

        /// Error message for invalid variable name.
        pub INVALID_VARNAME
        "%s: invalid variable name. See `help %s`"

        /// Error message on invalid combination of options.
        pub INVALID_OPT_COMBO
        "invalid option combination"

        pub INVALID_OPT_COMBO_WITH_CTX
        "invalid option combination, %s"

        pub COMBO_EXCLUSIVE
        "%s %s: options cannot be used together"

        pub REGEX_COMPILE
        "Regular expression compile error: %s"

        pub NO_SUITABLE_JOBS
        "There are no suitable jobs"

        pub COULD_NOT_FIND_JOB
        "Could not find job '%d'"

        pub STDIN_CLOSED
        "stdin is closed"

        pub INVALID_MAX_MATCHES
        "Invalid max matches value '%s'"

        pub INVALID_MAX_VALUE
        "Invalid max value '%s'"
    );

    #[must_use]
    pub fn new(msg: Cow<'a, wstr>) -> Self {
        Error {
            msg,
            cmd: Default::default(),
            subcmd: Default::default(),
            parser: Default::default(),
            hint: Default::default(),
        }
    }

    #[must_use]
    pub fn cmd(mut self, cmd: &'a wstr) -> Self {
        self.cmd = Some(cmd);
        self
    }

    #[must_use]
    pub fn subcmd(mut self, cmd: &'a wstr, subcmd: &'a wstr) -> Self {
        self.cmd = Some(cmd);
        self.subcmd = Some(subcmd);
        self
    }

    #[must_use]
    pub fn stacktrace(mut self, parser: &'a Parser) -> Self {
        self.parser = Some(parser);
        self
    }

    #[must_use]
    pub fn hint(mut self) -> Self {
        self.hint = true;
        self
    }

    // Convenience function for both stacktrace and hint
    #[must_use]
    pub fn full_trailer(self, parser: &'a Parser) -> Self {
        self.stacktrace(parser).hint()
    }

    #[must_use]
    pub fn append_to_msg(mut self, append: impl IntoCharIter) -> Self {
        self.append_assign_to_msg(append);
        self
    }
    pub fn append_assign_to_msg(&mut self, append: impl IntoCharIter) {
        let s = self.msg.to_mut();
        if !append.extend_wstring(s) {
            s.extend(append.chars());
        }
    }
    pub fn finish(self, streams: &mut IoStreams) {
        self.write_to(streams.err);
    }

    pub fn to_string(&self) -> WString {
        let mut out = OutputStream::String(StringOutputStream::new());
        self.write_to(&mut out);
        out.take()
    }

    pub fn write_to(&self, output: &mut OutputStream) {
        self.write_msg(output);
        self.write_stacktrace(output);
        self.write_hint(output);
    }

    fn write_msg(&self, output: &mut OutputStream) {
        let str: &wstr = match (self.cmd, self.subcmd) {
            (None, _) => &self.msg,
            (Some(cmd), None) => &wgettext_fmt!("%s: %s", cmd, &self.msg),
            (Some(cmd), Some(subcmd)) => &wgettext_fmt!("%s %s: %s", cmd, subcmd, &self.msg),
        };
        output.appendln(str);
    }

    fn write_stacktrace(&self, output: &mut OutputStream) {
        let Some(parser) = self.parser else {
            return;
        };
        let stacktrace = parser.current_line();
        if !stacktrace.is_empty() {
            output.append('\n');
            output.appendln(&stacktrace);
        }
    }

    fn write_hint(&self, output: &mut OutputStream) {
        if !self.hint {
            return;
        }
        let Some(cmd) = self.cmd else {
            return;
        };

        output.appendln(&wgettext_fmt!(
            "(Type 'help %s' for related documentation)",
            cmd
        ));
    }
}
