use super::*;

pub struct Trim<'args> {
    chars_to_trim: &'args wstr,
    left: bool,
    right: bool,
    quiet: bool,
}

impl Default for Trim<'_> {
    fn default() -> Self {
        Self {
            // from " \f\n\r\t\v"
            chars_to_trim: L!(" \x0C\n\r\x09\x0B"),
            left: false,
            right: false,
            quiet: false,
        }
    }
}

impl<'args> StringSubCommand<'args> for Trim<'args> {
    const LONG_OPTIONS: &'static [WOption<'static>] = &[
        wopt(L!("chars"), RequiredArgument, 'c'),
        wopt(L!("left"), NoArgument, 'l'),
        wopt(L!("right"), NoArgument, 'r'),
        wopt(L!("quiet"), NoArgument, 'q'),
    ];
    const SHORT_OPTIONS: &'static wstr = L!("c:lrq");

    fn parse_opt(
        &mut self,
        _n: &wstr,
        c: char,
        arg: Option<&'args wstr>,
    ) -> Result<(), StringError> {
        match c {
            'c' => self.chars_to_trim = arg.unwrap(),
            'l' => self.left = true,
            'r' => self.right = true,
            'q' => self.quiet = true,
            _ => return Err(StringError::UnknownOption),
        }
        Ok(())
    }

    fn handle(
        &mut self,
        _parser: &Parser,
        streams: &mut IoStreams,
        optind: &mut usize,
        args: &[&wstr],
    ) -> Result<(), ErrorCode> {
        // If neither left or right is specified, we do both.
        if !self.left && !self.right {
            self.left = true;
            self.right = true;
        }

        let mut ntrim = 0;

        let to_trim_end = |str: &wstr| -> usize {
            str.chars()
                .rev()
                .take_while(|&c| self.chars_to_trim.contains(c))
                .count()
        };

        let to_trim_start = |str: &wstr| -> usize {
            str.chars()
                .take_while(|&c| self.chars_to_trim.contains(c))
                .count()
        };

        for InputValue { arg, want_newline } in arguments(args, optind, streams) {
            let trim_start = if self.left { to_trim_start(&arg) } else { 0 };
            // collision is only an issue if the whole string is getting trimmed
            let trim_end = if self.right && trim_start != arg.len() {
                to_trim_end(&arg)
            } else {
                0
            };

            ntrim += trim_start + trim_end;
            if !self.quiet {
                streams.out.append(&arg[trim_start..arg.len() - trim_end]);
                if want_newline {
                    streams.out.append1('\n');
                }
            } else if ntrim > 0 {
                return Ok(());
            }
        }

        if ntrim > 0 {
            Ok(())
        } else {
            Err(STATUS_CMD_ERROR)
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::builtins::shared::{STATUS_CMD_ERROR, STATUS_CMD_OK};
    use crate::tests::prelude::*;
    use crate::validate;

    #[test]
    #[serial]
    #[rustfmt::skip]
    fn plain() {
        let _cleanup = test_init();
        validate!(["string", "trim"], STATUS_CMD_ERROR, "");
        validate!(["string", "trim", ""], STATUS_CMD_ERROR, "\n");
        validate!(["string", "trim", " "], STATUS_CMD_OK, "\n");
        validate!(["string", "trim", "  \x0C\n\r\t"], STATUS_CMD_OK, "\n");
        validate!(["string", "trim", " a"], STATUS_CMD_OK, "a\n");
        validate!(["string", "trim", "a "], STATUS_CMD_OK, "a\n");
        validate!(["string", "trim", " a "], STATUS_CMD_OK, "a\n");
        validate!(["string", "trim", "-l", " a"], STATUS_CMD_OK, "a\n");
        validate!(["string", "trim", "-l", "a "], STATUS_CMD_ERROR, "a \n");
        validate!(["string", "trim", "-l", " a "], STATUS_CMD_OK, "a \n");
        validate!(["string", "trim", "-r", " a"], STATUS_CMD_ERROR, " a\n");
        validate!(["string", "trim", "-r", "a "], STATUS_CMD_OK, "a\n");
        validate!(["string", "trim", "-r", " a "], STATUS_CMD_OK, " a\n");
        validate!(["string", "trim", "-c", ".", " a"], STATUS_CMD_ERROR, " a\n");
        validate!(["string", "trim", "-c", ".", "a "], STATUS_CMD_ERROR, "a \n");
        validate!(["string", "trim", "-c", ".", " a "], STATUS_CMD_ERROR, " a \n");
        validate!(["string", "trim", "-c", ".", ".a"], STATUS_CMD_OK, "a\n");
        validate!(["string", "trim", "-c", ".", "a."], STATUS_CMD_OK, "a\n");
        validate!(["string", "trim", "-c", ".", ".a."], STATUS_CMD_OK, "a\n");
        validate!(["string", "trim", "-c", "\\/", "/a\\"], STATUS_CMD_OK, "a\n");
        validate!(["string", "trim", "-c", "\\/", "a/"], STATUS_CMD_OK, "a\n");
        validate!(["string", "trim", "-c", "\\/", "\\a/"], STATUS_CMD_OK, "a\n");
        validate!(["string", "trim", "-c", "", ".a."], STATUS_CMD_ERROR, ".a.\n");
    }
}
