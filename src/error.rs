use std::borrow::Cow;

use super::prelude::*;
use crate::{io::IoStreams, parser::Parser, wgettext_fmt};

use fish_widestring::wstr;

#[macro_export]
macro_rules! err_fmt {
    (
        $string:literal // format string
        $(, $args:expr)* // list of expressions
        $(,)?   // optional trailing comma
    ) => {
        $crate::error::Error::new(
            $crate::wgettext_fmt!($string, $($args),*).into()
        )
    };
    (
        $string:expr // format string (LocalizableString)
        $(, $args:expr)* // list of expressions
        $(,)?   // optional trailing comma
    ) => {
        $crate::error::Error::new(
            $crate::wgettext_fmt!($string, $($args),*).into()
        )
    };
}
pub use err_fmt;

#[macro_export]
macro_rules! err_str {
    (
        $string:literal // format string
        $(,)?   // optional trailing comma
    ) => {
        $crate::error::Error::new(Cow::Borrowed($crate::wgettext!($string)))
    };
    (
        $string:expr // format string (LocalizableString)
        $(,)?   // optional trailing comma
    ) => {
        $crate::error::Error::new(Cow::Borrowed($crate::wgettext!(&$string)))
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
        $crate::error::Error::new($string.into())
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
        // Will feel as need later
    );

    pub fn new(msg: Cow<'a, wstr>) -> Self {
        Error {
            msg,
            cmd: Default::default(),
            subcmd: Default::default(),
            parser: Default::default(),
            hint: Default::default(),
        }
    }
    pub fn with_cmd(mut self, cmd: &'a wstr) -> Self {
        self.cmd = Some(cmd);
        self
    }
    pub fn with_subcmd(mut self, cmd: &'a wstr, subcmd: &'a wstr) -> Self {
        self.cmd = Some(cmd);
        self.subcmd = Some(subcmd);
        self
    }
    pub fn with_stacktrace(mut self, parser: &'a Parser) -> Self {
        self.parser = Some(parser);
        self
    }
    pub fn with_hint(mut self) -> Self {
        self.hint = true;
        self
    }
    // Convenience function for both stacktrace and hint
    pub fn with_full_trailer(self, parser: &'a Parser) -> Self {
        self.with_stacktrace(parser).with_hint()
    }

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
    pub fn finish(mut self, streams: &mut IoStreams) {
        self.print_msg(streams);
        self.print_stacktrace(streams);
        self.print_hint(streams);
    }

    fn print_msg(&mut self, streams: &mut IoStreams) {
        match (self.cmd, self.subcmd) {
            (None, _) => {
                streams.err.appendln(&self.msg);
            }
            (Some(cmd), None) => {
                streams
                    .err
                    .appendln(&wgettext_fmt!("%s: %s", cmd, &self.msg));
            }
            (Some(cmd), Some(subcmd)) => {
                streams
                    .err
                    .appendln(&wgettext_fmt!("%s: %s: %s", cmd, subcmd, &self.msg));
            }
        }
    }

    fn print_stacktrace(&mut self, streams: &mut IoStreams) {
        let Some(parser) = self.parser else {
            return;
        };
        let stacktrace = parser.current_line();
        if !stacktrace.is_empty() {
            streams.err.append('\n');
            streams.err.appendln(&stacktrace);
        }
    }

    fn print_hint(&mut self, streams: &mut IoStreams) {
        if !self.hint {
            return;
        }
        let Some(cmd) = self.cmd else {
            return;
        };

        streams.err.appendln(&wgettext_fmt!(
            "(Type 'help %s' for related documentation)",
            cmd
        ));
    }
}
