use super::*;

#[derive(Default)]
pub struct Length {
    quiet: bool,
    visible: bool,
}

impl StringSubCommand<'_> for Length {
    const LONG_OPTIONS: &'static [WOption<'static>] = &[
        wopt(L!("quiet"), NoArgument, 'q'),
        wopt(L!("visible"), NoArgument, 'V'),
    ];
    const SHORT_OPTIONS: &'static wstr = L!("qV");

    fn parse_opt(&mut self, _n: &wstr, c: char, _arg: Option<&wstr>) -> Result<(), StringError> {
        match c {
            'q' => self.quiet = true,
            'V' => self.visible = true,
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
        let mut nnonempty = 0usize;

        for InputValue { arg, .. } in arguments(args, optind, streams) {
            if self.visible {
                // Visible length only makes sense line-wise.
                for line in {
                    let val: &wstr = &arg;
                    val.split('\n')
                } {
                    let mut max = 0;
                    // Carriage-return returns us to the beginning. The longest substring without
                    // carriage-return determines the overall width.
                    for reset in {
                        let val = &line;
                        val.split('\r')
                    } {
                        let n = width_without_escapes(reset, 0);
                        max = usize::max(max, n);
                    }
                    if max > 0 {
                        nnonempty += 1;
                    }
                    if !self.quiet {
                        streams.out.appendln(max.to_wstring());
                    } else if nnonempty > 0 {
                        return Ok(());
                    }
                }
            } else {
                let n = arg.len();
                if n > 0 {
                    nnonempty += 1;
                }
                if !self.quiet {
                    streams.out.appendln(n.to_wstring());
                } else if nnonempty > 0 {
                    return Ok(());
                }
            }
        }
        if nnonempty > 0 {
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
        validate!(["string", "length"], STATUS_CMD_ERROR, "");
        validate!(["string", "length", ""], STATUS_CMD_ERROR, "0\n");
        validate!(["string", "length", "", "", ""], STATUS_CMD_ERROR, "0\n0\n0\n");
        validate!(["string", "length", "a"], STATUS_CMD_OK, "1\n");

        validate!(["string", "length", "\u{2008A}"], STATUS_CMD_OK, "1\n");
        validate!(["string", "length", "um", "dois", "três"], STATUS_CMD_OK, "2\n4\n4\n");
        validate!(["string", "length", "um", "dois", "três"], STATUS_CMD_OK, "2\n4\n4\n");
        validate!(["string", "length", "-q"], STATUS_CMD_ERROR, "");
        validate!(["string", "length", "-q", ""], STATUS_CMD_ERROR, "");
        validate!(["string", "length", "-q", "a"], STATUS_CMD_OK, "");
    }
}
