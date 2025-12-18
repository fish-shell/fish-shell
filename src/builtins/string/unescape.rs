use super::*;
use crate::common::{UnescapeStringStyle, unescape_string};

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

    fn parse_opt(&mut self, name: &wstr, c: char, arg: Option<&wstr>) -> Result<(), StringError> {
        match c {
            'n' => self.no_quoted = true,
            NON_OPTION_CHAR => {
                self.style = arg
                    .unwrap()
                    .try_into()
                    .map_err(|_| invalid_args!("%s: Invalid style value '%s'\n", name, arg))?
            }
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
        let mut nesc = 0;
        for InputValue { arg, want_newline } in arguments(args, optind, streams) {
            if let Some(res) = unescape_string(&arg, self.style) {
                streams.out.append(res);
                if want_newline {
                    streams.out.append1('\n');
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
