use super::*;
use fish_fallback::fish_wcwidth;

pub struct Pad {
    char_to_pad: char,
    pad_char_width: usize,
    pad_from: Direction,
    center: bool,
    width: usize,
}

impl Default for Pad {
    fn default() -> Self {
        Self {
            char_to_pad: ' ',
            pad_char_width: 1,
            pad_from: Direction::Left,
            center: false,
            width: 0,
        }
    }
}

impl StringSubCommand<'_> for Pad {
    const LONG_OPTIONS: &'static [WOption<'static>] = &[
        // FIXME docs say `--char`, there was no long_opt with `--char` in C++
        wopt(L!("chars"), RequiredArgument, 'c'),
        wopt(L!("right"), NoArgument, 'r'),
        wopt(L!("center"), NoArgument, 'C'),
        wopt(L!("width"), RequiredArgument, 'w'),
    ];
    const SHORT_OPTIONS: &'static wstr = L!("c:rCw:");

    fn parse_opt(&mut self, name: &wstr, c: char, arg: Option<&wstr>) -> Result<(), StringError> {
        match c {
            'c' => {
                let [pad_char] = arg.unwrap().as_char_slice() else {
                    return Err(invalid_args!(
                        "%s: Padding should be a character '%s'\n",
                        name,
                        arg
                    ));
                };
                let pad_char_width = fish_wcwidth(*pad_char);
                if pad_char_width <= 0 {
                    return Err(invalid_args!(
                        "%s: Invalid padding character of width zero '%s'\n",
                        name,
                        arg
                    ));
                }
                self.pad_char_width = pad_char_width as usize;
                self.char_to_pad = *pad_char;
            }
            'r' => self.pad_from = Direction::Right,
            'w' => {
                self.width = fish_wcstol(arg.unwrap())?
                    .try_into()
                    .map_err(|_| invalid_args!("%s: Invalid width value '%s'\n", name, arg))?
            }
            'C' => self.center = true,
            _ => return Err(StringError::UnknownOption),
        }
        Ok(())
    }

    fn handle<'args>(
        &mut self,
        _parser: &Parser,
        streams: &mut IoStreams,
        optind: &mut usize,
        args: &[&'args wstr],
    ) -> Result<(), ErrorCode> {
        let mut max_width = 0usize;
        let mut inputs: Vec<(Cow<'args, wstr>, usize)> = Vec::new();
        let mut print_trailing_newline = true;

        for InputValue { arg, want_newline } in arguments(args, optind, streams) {
            let width = width_without_escapes(&arg, 0);
            max_width = max_width.max(width);
            inputs.push((arg, width));
            print_trailing_newline = want_newline;
        }

        let pad_width = max_width.max(self.width);

        for (input, width) in inputs {
            let total_pad = pad_width - width;
            let (left_pad, right_pad) = match (self.pad_from, self.center) {
                (Direction::Left, false) => (total_pad, 0),
                (Direction::Right, false) => (0, total_pad),
                (Direction::Left, true) => (total_pad - total_pad / 2, total_pad / 2),
                (Direction::Right, true) => (total_pad / 2, total_pad - total_pad / 2),
            };

            let chars = |w| std::iter::repeat_n(self.char_to_pad, w / self.pad_char_width);
            let spaces = |w| std::iter::repeat_n(' ', w % self.pad_char_width);
            let mut padded: WString = chars(left_pad)
                .chain(spaces(left_pad))
                .chain(input.chars())
                .chain(spaces(right_pad))
                .chain(chars(right_pad))
                .collect();
            if print_trailing_newline {
                padded.push('\n');
            }

            streams.out.append(padded);
        }

        Ok(())
    }
}
