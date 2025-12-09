use std::num::NonZeroI64;

use super::*;

#[derive(Default)]
pub struct Sub {
    length: Option<usize>,
    quiet: bool,
    start: Option<NonZeroI64>,
    end: Option<NonZeroI64>,
}

impl StringSubCommand<'_> for Sub {
    const LONG_OPTIONS: &'static [WOption<'static>] = &[
        wopt(L!("length"), RequiredArgument, 'l'),
        wopt(L!("start"), RequiredArgument, 's'),
        wopt(L!("end"), RequiredArgument, 'e'),
        wopt(L!("quiet"), NoArgument, 'q'),
    ];
    const SHORT_OPTIONS: &'static wstr = L!("l:qs:e:");

    fn parse_opt(&mut self, name: &wstr, c: char, arg: Option<&wstr>) -> Result<(), StringError> {
        match c {
            'l' => {
                self.length = Some(
                    fish_wcstol(arg.unwrap())?
                        .try_into()
                        .map_err(|_| invalid_args!("%s: Invalid length value '%s'\n", name, arg))?,
                )
            }
            's' => {
                self.start = Some(
                    fish_wcstol(arg.unwrap())?
                        .try_into()
                        .map_err(|_| invalid_args!("%s: Invalid start value '%s'\n", name, arg))?,
                )
            }
            'e' => {
                self.end = Some(
                    fish_wcstol(arg.unwrap())?
                        .try_into()
                        .map_err(|_| invalid_args!("%s: Invalid end value '%s'\n", name, arg))?,
                )
            }
            'q' => self.quiet = true,
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
        let cmd = args[0];
        if self.length.is_some() && self.end.is_some() {
            streams.err.append(wgettext_fmt!(
                BUILTIN_ERR_COMBO2,
                cmd,
                wgettext!("--end and --length are mutually exclusive")
            ));
            return Err(STATUS_INVALID_ARGS);
        }

        let mut nsub = 0;
        for InputValue { arg, want_newline } in arguments(args, optind, streams) {
            let start: usize = match self.start.map(i64::from).unwrap_or_default() {
                n @ 1.. => n as usize - 1,
                0 => 0,
                n => {
                    let n = u64::min(n.unsigned_abs(), usize::MAX as u64) as usize;
                    arg.len().saturating_sub(n)
                }
            }
            .clamp(0, arg.len());

            let count = {
                let n = self
                    .end
                    .map(|e| match i64::from(e) {
                        // end can never be 0
                        n @ 1.. => n as usize,
                        n => {
                            let n = u64::min(n.unsigned_abs(), usize::MAX as u64) as usize;
                            arg.len().saturating_sub(n)
                        }
                    })
                    .map(|n| n.saturating_sub(start));

                self.length.or(n).unwrap_or(arg.len())
            };

            if !self.quiet {
                streams
                    .out
                    .append(&arg[start..usize::min(start + count, arg.len())]);
                if want_newline {
                    streams.out.append1('\n');
                }
            }
            nsub += 1;
            if self.quiet {
                return Ok(());
            }
        }

        if nsub > 0 {
            Ok(())
        } else {
            Err(STATUS_CMD_ERROR)
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::builtins::shared::{STATUS_CMD_ERROR, STATUS_CMD_OK, STATUS_INVALID_ARGS};
    use crate::tests::prelude::*;
    use crate::validate;

    #[test]
    #[serial]
    #[rustfmt::skip]
    fn plain() {
        let _cleanup = test_init();
        validate!(["string", "sub"], STATUS_CMD_ERROR, "");
        validate!(["string", "sub", "abcde"], STATUS_CMD_OK, "abcde\n");
        validate!(["string", "sub", "-l", "x", "abcde"], STATUS_INVALID_ARGS, "");
        validate!(["string", "sub", "-s", "x", "abcde"], STATUS_INVALID_ARGS, "");
        validate!(["string", "sub", "-l0", "abcde"], STATUS_CMD_OK, "\n");
        validate!(["string", "sub", "-l2", "abcde"], STATUS_CMD_OK, "ab\n");
        validate!(["string", "sub", "-l5", "abcde"], STATUS_CMD_OK, "abcde\n");
        validate!(["string", "sub", "-l6", "abcde"], STATUS_CMD_OK, "abcde\n");
        validate!(["string", "sub", "-l-1", "abcde"], STATUS_INVALID_ARGS, "");
        validate!(["string", "sub", "-s0", "abcde"], STATUS_INVALID_ARGS, "");
        validate!(["string", "sub", "-s1", "abcde"], STATUS_CMD_OK, "abcde\n");
        validate!(["string", "sub", "-s5", "abcde"], STATUS_CMD_OK, "e\n");
        validate!(["string", "sub", "-s6", "abcde"], STATUS_CMD_OK, "\n");
        validate!(["string", "sub", "-s-1", "abcde"], STATUS_CMD_OK, "e\n");
        validate!(["string", "sub", "-s-5", "abcde"], STATUS_CMD_OK, "abcde\n");
        validate!(["string", "sub", "-s-6", "abcde"], STATUS_CMD_OK, "abcde\n");
        validate!(["string", "sub", "-s1", "-l0", "abcde"], STATUS_CMD_OK, "\n");
        validate!(["string", "sub", "-s1", "-l1", "abcde"], STATUS_CMD_OK, "a\n");
        validate!(["string", "sub", "-s2", "-l2", "abcde"], STATUS_CMD_OK, "bc\n");
        validate!(["string", "sub", "-s-1", "-l1", "abcde"], STATUS_CMD_OK, "e\n");
        validate!(["string", "sub", "-s-1", "-l2", "abcde"], STATUS_CMD_OK, "e\n");
        validate!(["string", "sub", "-s-3", "-l2", "abcde"], STATUS_CMD_OK, "cd\n");
        validate!(["string", "sub", "-s-3", "-l4", "abcde"], STATUS_CMD_OK, "cde\n");
        validate!(["string", "sub", "-q"], STATUS_CMD_ERROR, "");
        validate!(["string", "sub", "-q", "abcde"], STATUS_CMD_OK, "");
    }
}
