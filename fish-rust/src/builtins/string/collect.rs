use super::*;

#[derive(Default)]
pub struct Collect {
    allow_empty: bool,
    no_trim_newlines: bool,
}

impl StringSubCommand<'_> for Collect {
    fn long_options(&self) -> &'static [woption<'static>] {
        const opts: &'static [woption<'static>] = &[
            wopt(L!("allow-empty"), no_argument, 'a'),
            wopt(L!("no-trim-newlines"), no_argument, 'N'),
        ];
        opts
    }
    fn short_options(&self) -> &'static wstr {
        L!(":Na")
    }

    fn parse_opt(&mut self, w: &mut wgetopter_t<'_, '_>, c: char) -> Result<(), StringError> {
        match c {
            'a' => self.allow_empty = true,
            'N' => self.no_trim_newlines = true,
            _ => return Err(StringError::UnknownOption),
        }
        Ok(())
    }

    fn handle(
        &mut self,
        _parser: &Parser,
        streams: &mut IoStreams<'_>,
        optind: &mut usize,
        args: &[WString],
    ) -> Option<libc::c_int> {
        let mut appended = 0usize;

        for (arg, want_newline) in
            arguments(args, optind, streams).with_split_behavior(SplitBehavior::Never)
        {
            let arg = if !self.no_trim_newlines {
                let trim_len = arg.len() - arg.chars().rev().take_while(|&c| c == '\n').count();
                &arg[..trim_len]
            } else {
                &arg
            };

            streams
                .out
                .append_with_separation(arg, SeparationType::explicitly, want_newline);
            appended += arg.len();
        }

        // If we haven't printed anything and "no_empty" is set,
        // print something empty. Helps with empty ellision:
        // echo (true | string collect --allow-empty)"bar"
        // prints "bar".
        if self.allow_empty && appended == 0 {
            streams.out.append_with_separation(
                L!(""),
                SeparationType::explicitly,
                true, /* historical behavior is to always print a newline */
            );
        }

        if appended > 0 {
            STATUS_CMD_OK
        } else {
            STATUS_CMD_ERROR
        }
    }
}
