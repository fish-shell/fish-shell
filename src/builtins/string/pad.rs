use super::*;
use crate::fallback::fish_wcwidth;

pub struct Pad {
    char_to_pad: char,
    pad_char_width: usize,
    pad_from: Direction,
    width: usize,
}

impl Default for Pad {
    fn default() -> Self {
        Self {
            char_to_pad: ' ',
            pad_char_width: 1,
            pad_from: Direction::Left,
            width: 0,
        }
    }
}

impl StringSubCommand<'_> for Pad {
    const LONG_OPTIONS: &'static [WOption<'static>] = &[
        // FIXME docs say `--char`, there was no long_opt with `--char` in C++
        wopt(L!("chars"), RequiredArgument, 'c'),
        wopt(L!("right"), NoArgument, 'r'),
        wopt(L!("width"), RequiredArgument, 'w'),
    ];
    const SHORT_OPTIONS: &'static wstr = L!(":c:rw:");

    fn parse_opt(&mut self, name: &wstr, c: char, arg: Option<&wstr>) -> Result<(), StringError> {
        match c {
            'c' => {
                let [pad_char] = arg.unwrap().as_char_slice() else {
                    return Err(invalid_args!(
                        "%ls: Padding should be a character '%ls'\n",
                        name,
                        arg
                    ));
                };
                let pad_char_width = fish_wcwidth(*pad_char);
                if pad_char_width <= 0 {
                    return Err(invalid_args!(
                        "%ls: Invalid padding character of width zero '%ls'\n",
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
                    .map_err(|_| invalid_args!("%ls: Invalid width value '%ls'\n", name, arg))?
            }
            _ => return Err(StringError::UnknownOption),
        }
        return Ok(());
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

        for (arg, want_newline) in arguments(args, optind, streams) {
            let width = width_without_escapes(&arg, 0);
            max_width = max_width.max(width);
            inputs.push((arg, width));
            print_trailing_newline = want_newline;
        }

        let pad_width = max_width.max(self.width);

        for (input, width) in inputs {
            use std::iter::repeat;

            let pad = (pad_width - width) / self.pad_char_width;
            let remaining_width = (pad_width - width) % self.pad_char_width;
            let mut padded: WString = match self.pad_from {
                Direction::Left => repeat(self.char_to_pad)
                    .take(pad)
                    .chain(repeat(' ').take(remaining_width))
                    .chain(input.chars())
                    .collect(),
                Direction::Right => input
                    .chars()
                    .chain(repeat(' ').take(remaining_width))
                    .chain(repeat(self.char_to_pad).take(pad))
                    .collect(),
            };

            if print_trailing_newline {
                padded.push('\n');
            }

            streams.out.append(padded);
        }

        Ok(())
    }
}
