use crate::env::environment::Environment;
use std::fs::Metadata;
use std::os::unix::prelude::{FileTypeExt, MetadataExt};
use std::time::SystemTime;

use super::prelude::*;
use crate::path::path_apply_working_directory;
use crate::wutil::{
    INVALID_FILE_ID, file_id_for_path, lwstat, normalize_path, waccess, wbasename, wdirname,
    wrealpath, wstat,
};
use bitflags::bitflags;
use fish_util::wcsfilecmp_glob;
use fish_wcstringutil::split_string_tok;
use libc::{PATH_MAX, S_ISGID, S_ISUID, mode_t};
use nix::unistd::{AccessFlags, Gid, Uid};

macro_rules! path_error {
    (
    $streams:expr,
    $string:expr
    $(, $args:expr)+
    $(,)?
    ) => {
        $streams.err.append(L!("path "));
        $streams.err.appendln(&wgettext_fmt!($string, $($args),*));
    };
}

fn path_unknown_option(parser: &Parser, streams: &mut IoStreams, subcmd: &wstr, opt: &wstr) {
    path_error!(streams, BUILTIN_ERR_UNKNOWN, subcmd, opt);
    builtin_print_error_trailer(parser, streams.err, L!("path"));
}

// How many bytes we read() at once.
// We use PATH_MAX here so we always get at least one path,
// and so we can automatically detect NULL-separated input.
const PATH_CHUNK_SIZE: usize = PATH_MAX as usize;
#[inline]
fn arguments<'iter, 'args>(
    args: &'iter [&'args wstr],
    optind: &'iter mut usize,
    streams: &mut IoStreams,
) -> Arguments<'args, 'iter> {
    Arguments::new(args, optind, streams, PATH_CHUNK_SIZE)
}

bitflags! {
    #[derive(Copy, Clone, Default)]
    pub struct TypeFlags: u32 {
        /// A block device
        const BLOCK = 1 << 0;
        /// A directory
        const DIR = 1 << 1;
        /// A regular file
        const FILE = 1 << 2;
        /// A link
        const LINK = 1 << 3;
        /// A character device
        const CHAR = 1 << 4;
        /// A fifo
        const FIFO = 1 << 5;
        /// A socket
        const SOCK = 1 << 6;
    }
}

impl TryFrom<&wstr> for TypeFlags {
    type Error = ();

    fn try_from(value: &wstr) -> Result<Self, Self::Error> {
        let flag = match value {
            t if t == "file" => Self::FILE,
            t if t == "dir" => Self::DIR,
            t if t == "block" => Self::BLOCK,
            t if t == "char" => Self::CHAR,
            t if t == "fifo" => Self::FIFO,
            t if t == "socket" => Self::SOCK,
            t if t == "link" => Self::LINK,
            _ => return Err(()),
        };

        Ok(flag)
    }
}

bitflags! {
    #[derive(Copy, Clone, Default)]
    pub struct PermFlags: u32 {
        const READ = 1 << 0;
        const WRITE = 1 << 1;
        const EXEC = 1 << 2;
        const SUID = 1 << 3;
        const SGID = 1 << 4;
        const USER = 1 << 5;
        const GROUP = 1 << 6;
    }
}

impl PermFlags {
    fn is_special(self) -> bool {
        self.intersects(Self::SUID | Self::SGID | Self::USER | Self::GROUP)
    }
}

impl TryFrom<&wstr> for PermFlags {
    type Error = ();

    fn try_from(value: &wstr) -> Result<Self, Self::Error> {
        let flag = match value {
            t if t == "read" => Self::READ,
            t if t == "write" => Self::WRITE,
            t if t == "exec" => Self::EXEC,
            t if t == "suid" => Self::SUID,
            t if t == "sgid" => Self::SGID,
            t if t == "user" => Self::USER,
            t if t == "group" => Self::GROUP,
            _ => return Err(()),
        };

        Ok(flag)
    }
}

/// This is used by the subcommands to communicate with the option parser which flags are
/// valid and get the result of parsing the command for flags.
#[derive(Default)]
struct Options<'args> {
    null_in: bool,
    null_out: bool,
    quiet: bool,

    invert_valid: bool,
    invert: bool,

    relative_valid: bool,
    relative: bool,

    reverse_valid: bool,
    reverse: bool,

    unique_valid: bool,
    unique: bool,

    key: Option<&'args wstr>,

    types_valid: bool,
    types: Option<TypeFlags>,

    perms_valid: bool,
    perms: Option<PermFlags>,

    arg1: Option<&'args wstr>,

    no_ext_valid: bool,
    no_ext: bool,

    all_valid: bool,
    all: bool,
}

#[inline]
fn path_out(streams: &mut IoStreams, opts: &Options<'_>, s: impl AsRef<wstr>) {
    let s = s.as_ref();
    if !opts.quiet {
        if !opts.null_out {
            streams
                .out
                .append_with_separation(s, SeparationType::explicitly, true);
        } else {
            let mut output = WString::with_capacity(s.len() + 1);
            output.push_utfstr(s);
            output.push('\0');
            streams.out.append(&output);
        }
    }
}

fn construct_short_opts(opts: &Options) -> WString {
    // All commands accept -z, -Z and -q
    let mut short_opts = WString::from("zZq");
    if opts.perms_valid {
        short_opts += L!("p:");
        short_opts += L!("rwx");
    }

    if opts.types_valid {
        short_opts += L!("t:");
        short_opts += L!("fld");
    }

    if opts.invert_valid {
        short_opts.push('v');
    }
    if opts.relative_valid {
        short_opts.push('R');
    }
    if opts.reverse_valid {
        short_opts.push('r');
    }
    if opts.unique_valid {
        short_opts.push('u');
    }
    if opts.no_ext_valid {
        short_opts.push('E');
    }
    short_opts
}

/// Note that several long flags share the same short flag. That is okay. The caller is expected
/// to indicate that a max of one of the long flags sharing a short flag is valid.
/// Remember: adjust the completions in share/completions/ when options change
const LONG_OPTIONS: [WOption<'static>; 12] = [
    wopt(L!("quiet"), NoArgument, 'q'),
    wopt(L!("null-in"), NoArgument, 'z'),
    wopt(L!("null-out"), NoArgument, 'Z'),
    wopt(L!("perm"), RequiredArgument, 'p'),
    wopt(L!("type"), RequiredArgument, 't'),
    wopt(L!("invert"), NoArgument, 'v'),
    wopt(L!("relative"), NoArgument, 'R'),
    wopt(L!("reverse"), NoArgument, 'r'),
    wopt(L!("unique"), NoArgument, 'u'),
    wopt(L!("key"), RequiredArgument, NON_OPTION_CHAR),
    wopt(L!("no-extension"), NoArgument, 'E'),
    wopt(L!("all"), NoArgument, '\x02'),
];

fn parse_opts<'args>(
    opts: &mut Options<'args>,
    optind: &mut usize,
    n_req_args: usize,
    args: &mut [&'args wstr],
    parser: &Parser,
    streams: &mut IoStreams,
) -> BuiltinResult {
    let cmd = args[0];
    let mut args_read = Vec::with_capacity(args.len());
    args_read.extend_from_slice(args);

    let short_opts = construct_short_opts(opts);

    let mut w = WGetopter::new(&short_opts, &LONG_OPTIONS, args);
    while let Some(c) = w.next_opt() {
        match c {
            ':' => {
                streams.err.append(L!("path ")); // clone of string_error
                builtin_missing_argument(parser, streams, cmd, args_read[w.wopt_index - 1], false);
                return Err(STATUS_INVALID_ARGS);
            }
            ';' => {
                streams.err.append(L!("path ")); // clone of string_error
                builtin_unexpected_argument(
                    parser,
                    streams,
                    cmd,
                    args_read[w.wopt_index - 1],
                    false,
                );
                return Err(STATUS_INVALID_ARGS);
            }
            '?' => {
                path_unknown_option(parser, streams, cmd, args_read[w.wopt_index - 1]);
                return Err(STATUS_INVALID_ARGS);
            }
            'q' => {
                opts.quiet = true;
                continue;
            }
            'z' => {
                opts.null_in = true;
                continue;
            }
            'Z' => {
                opts.null_out = true;
                continue;
            }
            'v' if opts.invert_valid => {
                opts.invert = true;
                continue;
            }
            't' if opts.types_valid => {
                let types = opts.types.get_or_insert_default();
                let types_args = split_string_tok(w.woptarg.unwrap(), L!(","), None);
                for t in types_args {
                    let Ok(r#type) = t.try_into() else {
                        path_error!(streams, "%s: Invalid type '%s'", "path", t);
                        return Err(STATUS_INVALID_ARGS);
                    };
                    *types |= r#type;
                }
                continue;
            }
            'p' if opts.perms_valid => {
                let perms = opts.perms.get_or_insert_default();
                let perms_args = split_string_tok(w.woptarg.unwrap(), L!(","), None);
                for p in perms_args {
                    let Ok(perm) = p.try_into() else {
                        path_error!(streams, "%s: Invalid permission '%s'", "path", p);
                        return Err(STATUS_INVALID_ARGS);
                    };
                    *perms |= perm;
                }
                continue;
            }
            'r' if opts.reverse_valid => {
                opts.reverse = true;
                continue;
            }
            'r' if opts.perms_valid => {
                let perms = opts.perms.get_or_insert_default();
                *perms |= PermFlags::READ;
                continue;
            }
            'w' if opts.perms_valid => {
                let perms = opts.perms.get_or_insert_default();
                *perms |= PermFlags::WRITE;
                continue;
            }
            'x' if opts.perms_valid => {
                let perms = opts.perms.get_or_insert_default();
                *perms |= PermFlags::EXEC;
                continue;
            }
            'f' if opts.types_valid => {
                let types = opts.types.get_or_insert_default();
                *types |= TypeFlags::FILE;
                continue;
            }
            'l' if opts.types_valid => {
                let types = opts.types.get_or_insert_default();
                *types |= TypeFlags::LINK;
                continue;
            }
            'd' if opts.types_valid => {
                let types = opts.types.get_or_insert_default();
                *types |= TypeFlags::DIR;
                continue;
            }
            'u' if opts.unique_valid => {
                opts.unique = true;
                continue;
            }
            'R' if opts.relative_valid => {
                opts.relative = true;
                continue;
            }
            'E' if opts.no_ext_valid => {
                opts.no_ext = true;
                continue;
            }
            NON_OPTION_CHAR => {
                assert!(w.woptarg.is_some());
                opts.key = w.woptarg;
                continue;
            }

            '\x02' if opts.all_valid => {
                opts.all = true;
                continue;
            }
            _ => {
                path_unknown_option(parser, streams, cmd, args_read[w.wopt_index - 1]);
                return Err(STATUS_INVALID_ARGS);
            }
        }
    }

    *optind = w.wopt_index;

    if n_req_args != 0 {
        assert_eq!(n_req_args, 1);
        opts.arg1 = args.get(*optind).copied();
        if opts.arg1.is_some() {
            *optind += 1;
        }

        if opts.arg1.is_none() && n_req_args == 1 {
            path_error!(streams, BUILTIN_ERR_ARG_COUNT0, cmd);
            return Err(STATUS_INVALID_ARGS);
        }
    }

    // At this point we should not have optional args and be reading args from stdin.
    if streams.stdin_is_directly_redirected && args.len() > *optind {
        path_error!(streams, BUILTIN_ERR_TOO_MANY_ARGUMENTS, cmd);
        return Err(STATUS_INVALID_ARGS);
    }

    Ok(SUCCESS)
}

fn path_transform(
    parser: &Parser,
    streams: &mut IoStreams,
    args: &mut [&wstr],
    func: impl Fn(&wstr) -> WString,
    custom_opts: impl Fn(&mut Options),
) -> BuiltinResult {
    let mut opts = Options::default();
    custom_opts(&mut opts);
    let mut optind = 0;

    parse_opts(&mut opts, &mut optind, 0, args, parser, streams)?;

    let mut n_transformed = 0;
    let arguments = arguments(args, &mut optind, streams).with_split_behavior(match opts.null_in {
        true => SplitBehavior::Null,
        false => SplitBehavior::InferNull,
    });
    for InputValue { arg, .. } in arguments {
        // Empty paths make no sense, but e.g. wbasename returns true for them.
        if arg.is_empty() {
            continue;
        }
        let mut transformed = func(&arg);
        if opts.no_ext {
            if let Some(idx) = find_extension(&transformed) {
                transformed.truncate(idx);
            }
        }
        if transformed != arg {
            n_transformed += 1;
            // Return okay if path wasn't already in this form
            // TODO: Is that correct?
            if opts.quiet {
                return Ok(SUCCESS);
            }
        }
        path_out(streams, &opts, transformed);
    }

    if n_transformed > 0 {
        Ok(SUCCESS)
    } else {
        Err(STATUS_CMD_ERROR)
    }
}

fn path_basename(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    path_transform(
        parser,
        streams,
        args,
        |s| wbasename(s).to_owned(),
        |opts| {
            opts.no_ext_valid = true;
        },
    )
}

fn path_dirname(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    path_transform(parser, streams, args, |s| wdirname(s).to_owned(), |_| {})
}

fn normalize_help(path: &wstr) -> WString {
    let mut np = normalize_path(path, false);
    if !np.is_empty() && np.char_at(0) == '-' {
        np = "./".chars().chain(np.chars()).collect();
    }
    np
}

fn path_normalize(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    path_transform(parser, streams, args, normalize_help, |_| {})
}

fn path_mtime(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let mut opts = Options {
        relative_valid: true,
        ..Default::default()
    };
    let mut optind = 0;

    parse_opts(&mut opts, &mut optind, 0, args, parser, streams)?;

    let mut n_transformed = 0;

    let t = match SystemTime::now().duration_since(SystemTime::UNIX_EPOCH) {
        Ok(dur) => dur.as_secs() as i64,
        Err(err) => -(err.duration().as_secs() as i64),
    };

    let arguments = arguments(args, &mut optind, streams).with_split_behavior(match opts.null_in {
        true => SplitBehavior::Null,
        false => SplitBehavior::InferNull,
    });
    for InputValue { arg, .. } in arguments {
        let ret = file_id_for_path(&arg);

        if ret != INVALID_FILE_ID {
            if opts.quiet {
                return Ok(SUCCESS);
            }
            n_transformed += 1;
            if !opts.relative {
                path_out(streams, &opts, (ret.mod_seconds).to_wstring());
            } else {
                // note that the mod time can actually be before the system time
                // so this can end up negative
                #[allow(clippy::unnecessary_cast)]
                path_out(streams, &opts, (t - ret.mod_seconds as i64).to_wstring());
            }
        }
    }

    if n_transformed > 0 {
        Ok(SUCCESS)
    } else {
        Err(STATUS_CMD_ERROR)
    }
}

fn find_extension(path: &wstr) -> Option<usize> {
    // The extension belongs to the basename,
    // if there is a "." before the last component it doesn't matter.
    // e.g. ~/.config/fish/conf.d/foo
    // does not have an extension! The ".d" here is not a file extension for "foo".
    // And "~/.config" doesn't have an extension either - the ".config" is the filename.
    let filename = wbasename(path);

    // "." and ".." aren't really *files* and therefore don't have an extension.
    if filename == "." || filename == ".." {
        return None;
    }

    // If we don't have a "." or the "." is the first in the filename,
    // we do not have an extension
    let pos = filename.chars().rposition(|c| c == '.');
    match pos {
        None | Some(0) => None,
        // Convert pos back to what it would be in the original path.
        Some(pos) => Some(pos + path.len() - filename.len()),
    }
}

fn path_extension(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let mut opts = Options::default();
    let mut optind = 0;

    parse_opts(&mut opts, &mut optind, 0, args, parser, streams)?;

    let mut n_transformed = 0;
    let arguments = arguments(args, &mut optind, streams).with_split_behavior(match opts.null_in {
        true => SplitBehavior::Null,
        false => SplitBehavior::InferNull,
    });
    for InputValue { arg, .. } in arguments {
        let pos = find_extension(&arg);
        let Some(pos) = pos else {
            // If there is no extension the extension is empty.
            // This is unambiguous because we include the ".".
            path_out(streams, &opts, L!(""));
            continue;
        };

        let ext = arg.slice_from(pos);
        if opts.quiet && !ext.is_empty() {
            return Ok(SUCCESS);
        }
        path_out(streams, &opts, ext);
        n_transformed += 1;
    }

    if n_transformed > 0 {
        Ok(SUCCESS)
    } else {
        Err(STATUS_CMD_ERROR)
    }
}

fn path_change_extension(
    parser: &Parser,
    streams: &mut IoStreams,
    args: &mut [&wstr],
) -> BuiltinResult {
    let mut opts = Options::default();
    let mut optind = 0;

    parse_opts(&mut opts, &mut optind, 1, args, parser, streams)?;

    let mut n_transformed = 0usize;
    let arguments = arguments(args, &mut optind, streams).with_split_behavior(match opts.null_in {
        true => SplitBehavior::Null,
        false => SplitBehavior::InferNull,
    });
    for InputValue { mut arg, .. } in arguments {
        let pos = find_extension(&arg);
        let mut ext = match pos {
            Some(pos) => {
                arg.to_mut().truncate(pos);
                arg.into_owned()
            }
            None => arg.into_owned(),
        };

        // Only add on the extension "." if we have something.
        // That way specifying an empty extension strips it.
        if let Some(replacement) = opts.arg1 {
            if !replacement.is_empty() {
                if replacement.char_at(0) != '.' {
                    ext.push('.');
                }
                ext.push_utfstr(replacement);
            }
        }
        path_out(streams, &opts, ext);

        n_transformed += 1;
    }

    if n_transformed > 0 {
        Ok(SUCCESS)
    } else {
        Err(STATUS_CMD_ERROR)
    }
}

fn path_resolve(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let mut opts = Options::default();
    let mut optind = 0;

    parse_opts(&mut opts, &mut optind, 0, args, parser, streams)?;

    let mut n_transformed = 0usize;
    let arguments = arguments(args, &mut optind, streams).with_split_behavior(match opts.null_in {
        true => SplitBehavior::Null,
        false => SplitBehavior::InferNull,
    });
    for InputValue { arg, .. } in arguments {
        let mut real = match wrealpath(&arg) {
            Some(p) => p,
            None => {
                // The path doesn't exist, isn't readable or a symlink loop.
                // We go up until we find something that works.
                let mut next = arg.into_owned();
                // First add $PWD if we're relative
                if !next.is_empty() && next.char_at(0) != '/' {
                    next = path_apply_working_directory(&next, &parser.vars().get_pwd_slash());
                }
                let mut rest = wbasename(&next).to_owned();
                let mut real = None;
                while !next.is_empty() && next != "/" {
                    next = wdirname(&next).to_owned();
                    real = wrealpath(&next);
                    if let Some(ref mut real) = real {
                        real.push('/');
                        real.push_utfstr(&rest);
                        *real = normalize_path(real, false);
                        break;
                    }
                    rest = (wbasename(&next).to_owned() + L!("/")) + rest.as_utfstr();
                }

                match real {
                    Some(p) => p,
                    None => continue,
                }
            }
        };

        // Normalize the path so "../" components are eliminated even after
        // nonexistent or non-directory components.
        // Otherwise `path resolve foo/../` will be `$PWD/foo/../` if foo is a file.
        real = normalize_path(&real, false);

        // Return 0 if we found a realpath.
        if opts.quiet {
            return Ok(SUCCESS);
        }
        path_out(streams, &opts, real);
        n_transformed += 1;
    }

    if n_transformed > 0 {
        Ok(SUCCESS)
    } else {
        Err(STATUS_CMD_ERROR)
    }
}

fn path_sort(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let mut opts = Options {
        reverse_valid: true,
        unique_valid: true,
        ..Default::default()
    };
    let mut optind = 0;

    parse_opts(&mut opts, &mut optind, 0, args, parser, streams)?;

    let keyfunc: fn(&wstr) -> &wstr = match &opts.key {
        Some(k) if k == "basename" => wbasename,
        Some(k) if k == "dirname" => wdirname,
        Some(k) if k == "path" => {
            // Act as if --key hadn't been given.
            opts.key = None;
            wbasename
        }
        None => wbasename,
        Some(k) => {
            path_error!(streams, "%s: Invalid sort key '%s'", args[0], k);
            return Err(STATUS_INVALID_ARGS);
        }
    };

    let arguments = arguments(args, &mut optind, streams).with_split_behavior(match opts.null_in {
        true => SplitBehavior::Null,
        false => SplitBehavior::InferNull,
    });

    let mut list: Vec<_> = arguments.map(|input_value| input_value.arg).collect();

    if opts.key.is_some() {
        // sort_by is stable
        list.sort_by(|a, b| match wcsfilecmp_glob(keyfunc(a), keyfunc(b)) {
            order if opts.reverse => order.reverse(),
            order => order,
        });

        if opts.unique {
            // we are sorted, dedup will remove all duplicates
            list.dedup_by(|a, b| keyfunc(a) == keyfunc(b));
        }
    } else {
        // Without --key, we just sort by the entire path,
        // so we have no need to transform and such.
        list.sort_by(|a, b| match wcsfilecmp_glob(a, b) {
            order if opts.reverse => order.reverse(),
            order => order,
        });

        if opts.unique {
            // we are sorted, dedup will remove all duplicates
            list.dedup();
        }
    }

    for entry in list {
        path_out(streams, &opts, &entry);
    }

    /* TODO: Return true only if already sorted? */
    Ok(SUCCESS)
}

fn filter_path(opts: &Options, path: &wstr, uid: Option<Uid>, gid: Option<Gid>) -> bool {
    // TODO: Add moar stuff:
    // fifos, sockets, size greater than zero, setuid, ...
    // Nothing to check, file existence is checked elsewhere.
    if opts.types.is_none() && opts.perms.is_none() {
        return true;
    }

    // We keep the metadata around for other checks if we have it.
    let mut metadata: Option<Metadata> = None;

    if let Some(t) = opts.types {
        let mut type_ok = false;
        if t.contains(TypeFlags::LINK) {
            let md = lwstat(path);
            type_ok = md.as_ref().is_ok_and(Metadata::is_symlink);
        }
        let Ok(md) = wstat(path) else {
            // Does not exist
            return false;
        };

        let ft = md.file_type();
        type_ok = match type_ok {
            true => true,
            _ if t.contains(TypeFlags::FILE) && ft.is_file() => true,
            _ if t.contains(TypeFlags::DIR) && ft.is_dir() => true,
            _ if t.contains(TypeFlags::BLOCK) && ft.is_block_device() => true,
            _ if t.contains(TypeFlags::CHAR) && ft.is_char_device() => true,
            _ if t.contains(TypeFlags::FIFO) && ft.is_fifo() => true,
            _ if t.contains(TypeFlags::SOCK) && ft.is_socket() => true,
            _ => false,
        };

        if !type_ok {
            return false;
        }
        metadata = Some(md);
    }

    if let Some(perm) = opts.perms {
        let mut amode = AccessFlags::empty();
        // TODO: Update bitflags so this works
        /*
        for f in perm {
            amode |= match f {
                PermFlags::READ => R_OK,
                PermFlags::WRITE => W_OK,
                PermFlags::EXEC => X_OK,
                _ => PermFlags::empty(),
            }
        }
        */
        if perm.contains(PermFlags::READ) {
            amode.insert(AccessFlags::R_OK);
        }
        if perm.contains(PermFlags::WRITE) {
            amode.insert(AccessFlags::W_OK);
        }
        if perm.contains(PermFlags::EXEC) {
            amode.insert(AccessFlags::X_OK);
        }
        // Skip this if we don't have a mode to check - the stat can do existence too.
        // It's tempting to check metadata here if we have it,
        // e.g. see if any read-bit is set for READ.
        // That won't work for root.
        if !amode.is_empty() && waccess(path, amode).is_err() {
            return false;
        }

        // Permissions that require special handling
        if perm.is_special() {
            let md = match metadata {
                Some(n) => n,
                _ => {
                    let Ok(md) = wstat(path) else {
                        return false;
                    };
                    md
                }
            };

            #[allow(clippy::if_same_then_else)]
            if perm.contains(PermFlags::SUID) && (md.mode() as mode_t & S_ISUID) == 0 {
                return false;
            } else if perm.contains(PermFlags::SGID) && (md.mode() as mode_t & S_ISGID) == 0 {
                return false;
            } else if perm.contains(PermFlags::USER) && uid.map(|u| u.as_raw()) != Some(md.uid()) {
                return false;
            } else if perm.contains(PermFlags::GROUP) && gid.map(|g| g.as_raw()) != Some(md.gid()) {
                return false;
            }
        }
    }

    // No filters failed.
    true
}

fn path_filter_maybe_is(
    parser: &Parser,
    streams: &mut IoStreams,
    args: &mut [&wstr],
    is_is: bool,
) -> BuiltinResult {
    let mut opts = Options {
        types_valid: true,
        perms_valid: true,
        invert_valid: true,
        all_valid: true,
        ..Default::default()
    };
    let mut optind = 0;

    parse_opts(&mut opts, &mut optind, 0, args, parser, streams)?;

    // If we have been invoked as "path is", which is "path filter -q".
    if is_is {
        opts.quiet = true;
    }

    let mut n_transformed = 0;
    let arguments = arguments(args, &mut optind, streams).with_split_behavior(match opts.null_in {
        true => SplitBehavior::Null,
        false => SplitBehavior::InferNull,
    });

    // If we're looking for the owner/group, get our euid/egid here once.
    let uid = if opts.perms.unwrap_or_default().contains(PermFlags::USER) {
        Some(Uid::effective())
    } else {
        None
    };
    let gid = if opts.perms.unwrap_or_default().contains(PermFlags::GROUP) {
        Some(Gid::effective())
    } else {
        None
    };

    // Collect arguments into a Vec so we can use .len()
    let arguments_vec: Vec<_> = arguments.collect();

    for InputValue { arg, .. } in arguments_vec.iter().filter(|&InputValue { arg, .. }| {
        (opts.perms.is_none() && opts.types.is_none())
            || (filter_path(&opts, arg, uid, gid) != opts.invert)
    }) {
        // If we don't have filters, check if it exists.
        if opts.perms.is_none() && opts.types.is_none() {
            let ok = waccess(arg, AccessFlags::F_OK).is_ok();
            if ok == opts.invert {
                // For --all, fail early if any path does not match the filter.
                if opts.all {
                    return Err(STATUS_CMD_ERROR);
                }
                continue;
            }
        }

        n_transformed += 1;

        if opts.all {
            // For --all, do not output paths, just check all must match.
            continue;
        }
        // We *know* this is a filename,
        // and so if it starts with a `-` we *know* it is relative
        // to $PWD. So we can add `./`.
        // Empty paths make no sense, but e.g. wbasename returns true for them.
        if !arg.is_empty() && arg.starts_with('-') {
            let out = WString::from("./") + arg.as_ref();
            path_out(streams, &opts, out);
        } else {
            path_out(streams, &opts, arg);
        }
        if opts.quiet {
            return Ok(SUCCESS);
        }
    }

    if opts.all && n_transformed != arguments_vec.len() {
        // We have a filter and some paths didn't match.
        // Return Err if we have a filter and some paths didn't match.
        return Err(STATUS_CMD_ERROR);
    }
    if n_transformed > 0 {
        Ok(SUCCESS)
    } else {
        Err(STATUS_CMD_ERROR)
    }
}

fn path_filter(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    path_filter_maybe_is(parser, streams, args, false)
}

fn path_is(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    path_filter_maybe_is(parser, streams, args, true)
}

/// The path builtin, for handling paths.
pub fn path(parser: &Parser, streams: &mut IoStreams, args: &mut [&wstr]) -> BuiltinResult {
    let Some(&cmd) = args.first() else {
        return Err(STATUS_INVALID_ARGS);
    };
    let argc = args.len();
    if argc <= 1 {
        streams
            .err
            .appendln(&wgettext_fmt!(BUILTIN_ERR_MISSING_SUBCMD, cmd));
        builtin_print_error_trailer(parser, streams.err, cmd);
        return Err(STATUS_INVALID_ARGS);
    }

    if args[1] == "-h" || args[1] == "--help" {
        builtin_print_help(parser, streams, cmd);
        return Ok(SUCCESS);
    }

    let subcmd_name = args[1];

    let subcmd: BuiltinCmd = match subcmd_name.to_string().as_str() {
        "basename" => path_basename,
        "change-extension" => path_change_extension,
        "dirname" => path_dirname,
        "extension" => path_extension,
        "filter" => path_filter,
        "is" => path_is,
        "mtime" => path_mtime,
        "normalize" => path_normalize,
        "resolve" => path_resolve,
        "sort" => path_sort,
        _ => {
            streams
                .err
                .appendln(&wgettext_fmt!(BUILTIN_ERR_INVALID_SUBCMD, cmd, subcmd_name));
            builtin_print_error_trailer(parser, streams.err, cmd);
            return Err(STATUS_INVALID_ARGS);
        }
    };

    if argc >= 3 && (args[2] == "-h" || args[2] == "--help") {
        // Unlike string, we don't have separate docs (yet)
        builtin_print_help(parser, streams, cmd);
        return Ok(SUCCESS);
    }
    let args = &mut args[1..];
    subcmd(parser, streams, args)
}

#[cfg(test)]
mod tests {
    use super::find_extension;
    use crate::prelude::*;

    #[test]
    fn test_find_extension() {
        let cases = [
            (L!("foo.wmv"), Some(3)),
            (L!("verylongfilename.wmv"), Some("verylongfilename".len())),
            (L!("foo"), None),
            (L!(".foo"), None),
            (L!("./foo.wmv"), Some(5)),
        ];

        for (f, ext_idx) in cases {
            assert_eq!(find_extension(f), ext_idx);
        }
    }
}
