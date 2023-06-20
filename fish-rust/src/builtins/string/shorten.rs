use super::*;
use crate::common::get_ellipsis_str;
use crate::fallback::fish_wcwidth;
use crate::wcstringutil::split_string;
use crate::wutil::{fish_wcstol, fish_wcswidth};

pub struct Shorten<'args> {
    chars_to_shorten: &'args wstr,
    max: Option<usize>,
    no_newline: bool,
    quiet: bool,
    direction: Direction,
}

impl Default for Shorten<'_> {
    fn default() -> Self {
        Self {
            chars_to_shorten: get_ellipsis_str(),
            max: None,
            no_newline: false,
            quiet: false,
            direction: Direction::Right,
        }
    }
}

impl<'args> StringSubCommand<'args> for Shorten<'args> {
    const LONG_OPTIONS: &'static [woption<'static>] = &[
        // FIXME: documentation says it's --char
        wopt(L!("chars"), required_argument, 'c'),
        wopt(L!("max"), required_argument, 'm'),
        wopt(L!("no-newline"), no_argument, 'N'),
        wopt(L!("left"), no_argument, 'l'),
        wopt(L!("quiet"), no_argument, 'q'),
    ];
    const SHORT_OPTIONS: &'static wstr = L!(":c:m:Nlq");

    fn parse_opt(
        &mut self,
        name: &wstr,
        c: char,
        arg: Option<&'args wstr>,
    ) -> Result<(), StringError> {
        match c {
            'c' => self.chars_to_shorten = arg.expect("option --char requires an argument"),
            'm' => {
                self.max = Some(
                    fish_wcstol(arg.unwrap())?
                        .try_into()
                        .map_err(|_| invalid_args!("%ls: Invalid max value '%ls'\n", name, arg))?,
                )
            }
            'N' => self.no_newline = true,
            'l' => self.direction = Direction::Left,
            'q' => self.quiet = true,
            _ => return Err(StringError::UnknownOption),
        }
        return Ok(());
    }

    fn handle(
        &mut self,
        _parser: &mut parser_t,
        streams: &mut io_streams_t,
        optind: &mut usize,
        args: &[&wstr],
    ) -> Option<libc::c_int> {
        let mut min_width = usize::MAX;
        let mut inputs = Vec::new();
        let mut ell = self.chars_to_shorten;

        let iter = Arguments::new(args, optind, streams);

        if self.max == Some(0) {
            // Special case: Max of 0 means no shortening.
            // This makes this more reusable, so you don't need special-cases like
            //
            // if test $shorten -gt 0
            //     string shorten -m $shorten whatever
            // else
            //     echo whatever
            // end
            for (arg, _) in iter {
                streams.out.append(arg);
                streams.out.append1('\n');
            }
            return STATUS_CMD_OK;
        }

        for (arg, _) in iter {
            // Visible width only makes sense line-wise.
            // So either we have no-newlines (which means we shorten on the first newline),
            // or we handle the lines separately.
            let mut splits = split_string(&arg, '\n').into_iter();
            if self.no_newline && splits.len() > 1 {
                let mut s = match self.direction {
                    Direction::Right => splits.next(),
                    Direction::Left => splits.last(),
                }
                .unwrap();
                s.push_utfstr(ell);
                let width = width_without_escapes(&s, 0);

                if width > 0 && (width as usize) < min_width {
                    min_width = width as usize;
                }
                inputs.push(s);
            } else {
                for s in splits {
                    let width = width_without_escapes(&s, 0);
                    if width > 0 && (width as usize) < min_width {
                        min_width = width as usize;
                    }
                    inputs.push(s);
                }
            }
        }

        let ourmax: usize = self.max.unwrap_or(min_width);

        // TODO: Can we have negative width

        let ell_width: i32 = {
            let w = fish_wcswidth(ell);
            if w > ourmax as i32 {
                // If we can't even print our ellipsis, we substitute nothing,
                // truncating instead.
                ell = L!("");
                0
            } else {
                w
            }
        };

        let mut nsub = 0usize;
        // We could also error out here if the width of our ellipsis is larger
        // than the target width.
        // That seems excessive - specifically because the ellipsis on LANG=C
        // is "..." (width 3!).

        let skip_escapes = |l: &wstr, pos: usize| -> usize {
            let mut totallen = 0usize;
            while l.char_at(pos + totallen) == '\x1B' {
                let Some(len) = escape_code_length(l.slice_from(pos + totallen)) else {
                    break;
                };
                totallen += len;
            }
            totallen
        };

        for line in inputs {
            let mut pos = 0usize;
            let mut max = 0usize;
            // Collect how much of the string we can use without going over the maximum.
            if self.direction == Direction::Left {
                // Our strategy for keeping from the end.
                // This is rather unoptimized - actually going *backwards* from the end
                // is extremely tricky because we would have to subtract escapes again.
                // Also we need to avoid hacking combiners into bits.
                // This should work for most cases considering the combiners typically have width 0.
                let mut out = L!("");
                while pos < line.len() {
                    let w = width_without_escapes(&line, pos);
                    // If we're at the beginning and it fits, we sits.
                    //
                    // Otherwise we require it to fit the ellipsis
                    if (w <= ourmax as i32 && pos == 0) || (w + ell_width <= ourmax as i32) {
                        out = line.slice_from(pos);
                        break;
                    }

                    pos += skip_escapes(&line, pos).max(1);
                }
                if self.quiet && pos != 0 {
                    return STATUS_CMD_OK;
                }

                let output = match pos {
                    0 => line,
                    _ => {
                        // We have an ellipsis, construct our string and print it.
                        nsub += 1;
                        let mut res = WString::with_capacity(ell.len() + out.len());
                        res.push_utfstr(ell);
                        res.push_utfstr(out);
                        res
                    }
                };
                streams.out.append(output);
                streams.out.append1('\n');
                continue;
            } else {
                /* Direction::Right */
                // Going from the left.
                // This is somewhat easier.
                while max <= ourmax && pos < line.len() {
                    pos += skip_escapes(&line, pos);
                    let w = fish_wcwidth(line.char_at(pos));
                    if w <= 0 || max + w as usize + ell_width as usize <= ourmax {
                        // If it still fits, even if it is the last, we add it.
                        max += w as usize;
                        pos += 1;
                    } else {
                        // We're at the limit, so see if the entire string fits.
                        let mut max2: usize = max + w as usize;
                        let mut pos2 = pos + 1;
                        while pos2 < line.len() {
                            pos2 += skip_escapes(&line, pos2);
                            max2 += fish_wcwidth(line.char_at(pos2)) as usize;
                            pos2 += 1;
                        }

                        if max2 <= ourmax {
                            // We're at the end and everything fits,
                            // no ellipsis.
                            pos = pos2;
                        }
                        break;
                    }
                }
            }

            if self.quiet && pos != line.len() {
                return STATUS_CMD_OK;
            }

            if pos == line.len() {
                streams.out.append(line);
                streams.out.append1('\n');
                continue;
            }

            nsub += 1;
            let mut newl = line;
            newl.truncate(pos);
            newl.push_utfstr(ell);
            newl.push('\n');
            streams.out.append(newl);
        }

        // Return true if we have shortened something and false otherwise.
        if nsub > 0 {
            STATUS_CMD_OK
        } else {
            STATUS_CMD_ERROR
        }
    }
}
