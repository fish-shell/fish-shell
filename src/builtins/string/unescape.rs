use super::*;
use fish_common::{UnescapeStringStyle, unescape_string};

#[derive(Default)]
pub struct Unescape {
    no_quoted: bool,
    style: UnescapeStringStyle,
}

impl StringSubCommand<'_> for Unescape {
    const LONG_OPTIONS: &'static [WOption<'static>] = &[
        // FIXME: this flag means nothing, but was present in the C++ code
        // should be removed
        wopt(L!("no-quoted"), NoArgument, 'n'),
        wopt(L!("style"), RequiredArgument, NON_OPTION_CHAR),
    ];
    const SHORT_OPTIONS: &'static wstr = L!("n");

    fn parse_opt(&mut self, c: char, arg: Option<&wstr>) -> Result<(), StringError<'_>> {
        match c {
            'n' => self.no_quoted = true,
            NON_OPTION_CHAR => {
                let arg = arg.unwrap();
                self.style = arg
                    .try_into()
                    .map_err(|_| err_fmt!("Invalid style value '%s'", arg))?;
            }
            _ => return Err(StringError::UnknownOption),
        }
        Ok(())
    }

    fn handle(
        &mut self,
        _parser: &mut Parser,
        streams: &mut IoStreams,
        optind: &mut usize,
        args: &[&wstr],
    ) -> Result<(), ErrorCode> {
        let mut nesc = 0;
        for InputValue { arg, want_newline } in arguments(args, optind, streams) {
            if let Some(res) = unescape_string(&arg, self.style) {
                streams.out.append(&res);
                if want_newline {
                    streams.out.append('\n');
                }
                nesc += 1;
            }
        }

        if nesc > 0 {
            Ok(())
        } else {
            Err(STATUS_CMD_ERROR)
        }
    }
}
