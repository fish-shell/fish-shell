use super::*;
use crate::common::{EscapeFlags, EscapeStringStyle, escape_string};

#[derive(Default)]
pub struct Escape {
    no_quoted: bool,
    style: EscapeStringStyle,
}

impl StringSubCommand<'_> for Escape {
    const LONG_OPTIONS: &'static [WOption<'static>] = &[
        wopt(L!("no-quoted"), NoArgument, 'n'),
        wopt(L!("style"), RequiredArgument, NON_OPTION_CHAR),
    ];
    const SHORT_OPTIONS: &'static wstr = L!("n");

    fn parse_opt(&mut self, name: &wstr, c: char, arg: Option<&wstr>) -> Result<(), StringError> {
        match c {
            'n' => self.no_quoted = true,
            NON_OPTION_CHAR => {
                self.style = arg
                    .unwrap()
                    .try_into()
                    .map_err(|_| invalid_args!("%s: Invalid escape style '%s'\n", name, arg))?
            }
            _ => return Err(StringError::UnknownOption),
        }
        return Ok(());
    }

    fn handle(
        &mut self,
        _parser: &Parser,
        streams: &mut IoStreams,
        optind: &mut usize,
        args: &[&wstr],
    ) -> Result<(), ErrorCode> {
        // Currently, only the script style supports options.
        // Ignore them for other styles for now.
        let style = match self.style {
            EscapeStringStyle::Script(..) if self.no_quoted => {
                EscapeStringStyle::Script(EscapeFlags::NO_QUOTED)
            }
            x => x,
        };

        let mut escaped_any = false;
        for (arg, want_newline) in arguments(args, optind, streams) {
            let mut escaped = escape_string(&arg, style);

            if want_newline {
                escaped.push('\n');
            }

            streams.out.append(escaped);
            escaped_any = true;
        }

        if escaped_any {
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
        validate!(["string", "escape"], STATUS_CMD_ERROR, "");
        validate!(["string", "escape", ""], STATUS_CMD_OK, "''\n");
        validate!(["string", "escape", "-n", ""], STATUS_CMD_OK, "\n");
        validate!(["string", "escape", "a"], STATUS_CMD_OK, "a\n");
        validate!(["string", "escape", "\x07"], STATUS_CMD_OK, "\\cg\n");
        validate!(["string", "escape", "\"x\""], STATUS_CMD_OK, "'\"x\"'\n");
        validate!(["string", "escape", "hello world"], STATUS_CMD_OK, "'hello world'\n");
        validate!(["string", "escape", "-n", "hello world"], STATUS_CMD_OK, "hello\\ world\n");
        validate!(["string", "escape", "hello", "world"], STATUS_CMD_OK, "hello\nworld\n");
        validate!(["string", "escape", "-n", "~"], STATUS_CMD_OK, "\\~\n");
    }
}
