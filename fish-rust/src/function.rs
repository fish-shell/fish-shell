//! Functions for storing and retrieving function information. These functions also take care of
//! autoloading functions in the $fish_function_path. Actual function evaluation is taken care of by
//! the parser and to some degree the builtin handling library.

use crate::ast::Node;
use crate::autoload::Autoload;
use crate::common::{escape, valid_func_name, FilenameRef};
use crate::complete::complete_get_wrap_targets;
use crate::env::EnvStack;
use crate::event::EventType;
use crate::parse_tree::ParsedSourceRef;
use crate::parser::Parser;
use crate::parser_keywords::parser_keywords_is_reserved;
use crate::pointer::ConstPointer;
use crate::wchar::{wstr, WString, L};
use crate::wchar_ext::WExt;
use crate::wutil::DirIter;
use crate::{ast, event};
use once_cell::sync::Lazy;
use printf_compat::sprintf;
use std::collections::{HashMap, HashSet};
use std::sync::{Arc, Mutex, RwLock};
use widestring_suffix::widestrs;

/// A function's constant properties. These do not change once initialized.
#[derive(Clone, Default)]
pub struct FunctionProperties {
    /// Parsed source containing the function.
    pub parsed_source: Option<ParsedSourceRef>,

    /// Node containing the function statement, pointing into parsed_source.
    /// We store block_statement, not job_list, so that comments attached to the header are
    /// preserved.
    pub func_node: ConstPointer<ast::BlockStatement>,

    /// List of all named arguments for this function.
    pub named_arguments: Vec<WString>,

    /// Description of the function.
    pub description: WString,

    /// Mapping of all variables that were inherited from the function definition scope to their
    /// values.
    pub inherit_vars: HashMap<WString, Vec<WString>>,

    /// Set to true if invoking this function shadows the variables of the underlying function.
    pub shadow_scope: bool,

    /// Whether the function was autoloaded.
    pub is_autoload: bool,

    /// The file from which the function was created, or nullptr if not from a file.
    pub definition_file: Option<FilenameRef>,

    /// Whether the function was copied.
    pub is_copy: bool,

    /// The file from which the function was copied, or nullptr if not from a file.
    pub copy_definition_file: FilenameRef,

    /// The line number where the specified function was copied.
    pub copy_definition_lineno: usize,
}

impl FunctionProperties {
    pub fn new() -> Self {
        Self {
            shadow_scope: true,
            is_autoload: false,
            is_copy: false,
            ..Default::default()
        }
    }

    /// \return the description, localized via _.
    pub fn localized_description(&self) -> &wstr {
        if self.description.is_empty() {
            return L!("");
        }
        // TODO The translation got lost in the Rust port.
        &self.description
    }

    /// \return the line number where the definition of the specified function started.
    pub fn definition_lineno(&self) -> usize {
        // return one plus the number of newlines at offsets less than the start of our function's
        // statement (which includes the header).
        // TODO: merge with line_offset_of_character_at_offset?
        let source_range = self
            .func_node
            .try_source_range()
            .expect("Function has no source range");
        let func_start = source_range.start();
        let source = &self.parsed_source.as_ref().unwrap().src;
        assert!(func_start <= source.len(), "function start out of bounds");
        1 + source.chars().filter(|c| *c == '\n').count()
    }

    /// \return a definition of the function, annotated with properties like event handlers and wrap
    /// targets. This is to support the 'functions' builtin.
    /// Note callers must provide the function name, since the function does not know its own name.
    #[widestrs]
    pub fn annotated_definition(&self, name: &wstr) -> WString {
        let mut out = WString::new();
        let desc = self.localized_description();
        let def = get_function_body_source(self);
        let handlers = event::get_function_handlers(name);

        out.push_utfstr("function "L);

        // Typically we prefer to specify the function name first, e.g. "function foo --description bar"
        // But if the function name starts with a -, we'll need to output it after all the options.
        let defer_function_name = name.as_char_slice().first() == Some(&'-');
        if !defer_function_name {
            out.push_utfstr(&escape(name))
        }

        // Output wrap targets.
        for wrap in complete_get_wrap_targets(name) {
            out.push_utfstr(" --wraps="L);
            out.push_utfstr(&escape(&wrap));
        }

        if !desc.is_empty() {
            out.push_utfstr(" --description "L);
            out.push_utfstr(&escape(desc));
        }

        if !self.shadow_scope {
            out.push_utfstr(" --no-scope-shadowing"L);
        }

        for d in handlers {
            match &d.desc.typ {
                EventType::Signal { signal } => {
                    out.push_utfstr(&sprintf!(" --on-signal %ls", signal.desc()));
                }
                EventType::Variable { name } => {
                    out.push_utfstr(&sprintf!(" --on-variable %ls", name));
                }
                EventType::ProcessExit { pid } => {
                    out.push_utfstr(&sprintf!(" --on-process-exit %d", pid));
                }
                EventType::JobExit {
                    pid,
                    internal_job_id,
                } => {
                    out.push_utfstr(&sprintf!(" --on-job-exit %ls", pid));
                }
                EventType::CallerExit { caller_id } => {
                    out.push_utfstr(" --on-job-exit caller"L);
                }
                EventType::Generic { param } => {
                    out.push_utfstr(&sprintf!(" --on-event %ls", param));
                }
                EventType::Any => {
                    panic!("unexpected next->typ");
                }
            }
        }

        let mut named = &self.named_arguments;
        if !named.is_empty() {
            out.push_utfstr(" --argument"L);
            for name in named {
                out.push_utfstr(&sprintf!(" %ls", name));
            }
        }

        // Output the function name if we deferred it.
        if defer_function_name {
            out.push_utfstr(" -- "L);
            out.push_utfstr(&escape(name));
        }

        // Output any inherited variables as `set -l` lines.
        for (key, value) in &self.inherit_vars {
            // We don't know what indentation style the function uses,
            // so we do what fish_indent would.
            out.push_utfstr(&sprintf!("\n    set -l %ls"L, key));
            for arg in value {
                out.push(' ');
                out.push_utfstr(&escape(arg));
            }
        }
        out.push('\n');
        out.push_utfstr(&def);

        // Append a newline before the 'end', unless there already is one there.
        if !def.ends_with("\n"L) {
            out.push('\n');
        }
        out.push_utfstr("end\n"L);
        out
    }
}

type FunctionPropertiesRef = Arc<RwLock<FunctionProperties>>;

/// Add a function. This may mutate \p props to set is_autoload.
pub fn function_add(name: WString, props: FunctionPropertiesRef) {
    let mut funcset = FUNCTION_SET.lock().unwrap();

    // Historical check. TODO: rationalize this.
    if name.is_empty() {
        return;
    }

    // Remove the old function.
    funcset.remove(&name);

    // Check if this is a function that we are autoloading.
    props.write().unwrap().is_autoload = funcset.autoloader.autoload_in_progress(&name);

    // Create and store a new function.
    let old = funcset.funcs.insert(name, props);
    assert!(
        old.is_none(),
        "Function should not already be present in the table"
    );
}

/// Remove the function with the specified name.
pub fn function_remove(name: &wstr) {
    let mut funcset = FUNCTION_SET.lock().unwrap();
    funcset.remove(name);
    // Prevent (re-)autoloading this function.
    funcset.autoload_tombstones.insert(name.to_owned());
}

/// \return the properties for a function, or nullptr if none. This does not trigger autoloading.
pub fn function_get_props(name: &wstr) -> Option<FunctionPropertiesRef> {
    if parser_keywords_is_reserved(name) {
        return None;
    }
    FUNCTION_SET.lock().unwrap().get_props(name)
}

/// \return the properties for a function, or nullptr if none, perhaps triggering autoloading.
pub fn function_get_props_autoload(
    name: &wstr,
    parser: &mut Parser,
) -> Option<FunctionPropertiesRef> {
    parser.assert_can_execute();
    if parser_keywords_is_reserved(name) {
        return None;
    }
    function_load(name, parser);
    function_get_props(name)
}

/// Try autoloading a function.
/// \return true if something new was autoloaded, false if it was already loaded or did not exist.
/// Make sure that if the specified function is a dynamically loaded function, it has been fully
/// loaded.
/// Note this executes fish script code.
pub fn function_load(name: &wstr, parser: &mut Parser) -> bool {
    parser.assert_can_execute();
    let mut path_to_autoload = None;
    // Note we can't autoload while holding the funcset lock.
    // Lock around a local region.
    {
        let mut funcset = FUNCTION_SET.lock().unwrap();
        if funcset.allow_autoload(name) {
            path_to_autoload = funcset
                .autoloader
                .resolve_command(name, EnvStack::globals());
        }
    }

    // Release the lock and perform any autoload, then reacquire the lock and clean up.
    if let Some(path) = path_to_autoload {
        // Crucially, the lock is acquired after perform_autoload().
        Autoload::perform_autoload(&path, parser);
        FUNCTION_SET
            .lock()
            .unwrap()
            .autoloader
            .mark_autoload_finished(name);
        return true;
    }
    false
}

/// Sets the description of the function with the name \c name.
/// This triggers autoloading.
pub fn function_set_desc(name: &wstr, desc: &wstr, parser: &mut Parser) {
    parser.assert_can_execute();
    function_load(name, parser);
    let mut funcset = FUNCTION_SET.lock().unwrap();
    if let Some(props) = funcset.funcs.get_mut(name) {
        // Note the description is immutable, as it may be accessed on another thread, so we copy
        // the properties to modify it.
        let new_props = copy_props(props);
        new_props.write().unwrap().description = desc.to_owned();
        *props = new_props;
    }
}

/// Returns true if the function named \p cmd exists.
/// This may autoload.
pub fn function_exists(cmd: &wstr, parser: &mut Parser) -> bool {
    parser.assert_can_execute();
    if !valid_func_name(cmd) {
        return false;
    }
    function_get_props_autoload(cmd, parser).is_some()
}

/// Returns true if the function \p cmd either is loaded, or exists on disk in an autoload
/// directory.
pub fn function_exists_no_autoload(cmd: &wstr) -> bool {
    if !valid_func_name(cmd) {
        return false;
    }
    if parser_keywords_is_reserved(cmd) {
        return false;
    }
    let mut funcset = FUNCTION_SET.lock().unwrap();

    // Check if we either have the function, or it could be autoloaded.
    funcset.get_props(cmd).is_some() || funcset.autoloader.can_autoload(cmd)
}

/// Returns all function names.
///
/// \param get_hidden whether to include hidden functions, i.e. ones starting with an underscore.
pub fn function_get_names(get_hidden: bool) -> Vec<WString> {
    let mut names = HashSet::new();
    let funcset = FUNCTION_SET.lock().unwrap();
    autoload_names(&mut names, get_hidden);
    for name in funcset.funcs.keys() {
        // Maybe skip hidden.
        if !get_hidden && name.as_char_slice().first() == Some(&'_') {
            continue;
        }
        names.insert(name.clone());
    }
    names.into_iter().collect()
}

/// Creates a new function using the same definition as the specified function. Returns true if copy
/// is successful.
pub fn function_copy(name: &wstr, new_name: WString, parser: &mut Parser) -> bool {
    let filename = parser.current_filename();
    let lineno = parser.get_lineno();

    let mut funcset = FUNCTION_SET.lock().unwrap();
    let Some(props) = funcset.get_props(name) else {
        // No such function.
        return false;
    };
    // Copy the function's props.
    let mut new_props = copy_props(&props);
    {
        let mut new_props = new_props.write().unwrap();
        new_props.is_autoload = false;
        new_props.is_copy = true;
        new_props.copy_definition_file = filename;
        new_props.copy_definition_lineno = lineno;
    }

    // Note this will NOT overwrite an existing function with the new name.
    // TODO: rationalize if this behavior is desired.
    funcset.funcs.insert(new_name, new_props);
    true
}

/// Observes that fish_function_path has changed.
pub fn function_invalidate_path() {
    // Remove all autoloaded functions and update the autoload path.
    // Note we don't want to risk removal during iteration; we expect this to be called
    // infrequently.
    let mut funcset = FUNCTION_SET.lock().unwrap();
    let mut autoloadees = vec![];
    for (key, value) in &funcset.funcs {
        if value.read().unwrap().is_autoload {
            autoloadees.push(key.to_owned());
        }
    }
    for name in autoloadees {
        funcset.remove(&name);
    }
    funcset.autoloader.clear();
}

/// Type wrapping up the set of all functions.
/// There's only one of these; it's managed by a lock.
struct FunctionSet {
    /// The map of all functions by name.
    funcs: HashMap<WString, FunctionPropertiesRef>,

    /// Tombstones for functions that should no longer be autoloaded.
    autoload_tombstones: HashSet<WString>,

    /// The autoloader for our functions.
    autoloader: Autoload,
}

impl FunctionSet {
    fn new() -> Self {
        Self {
            funcs: HashMap::new(),
            autoload_tombstones: HashSet::new(),
            autoloader: Autoload::new(L!("fish_function_path")),
        }
    }
    /// Remove a function.
    /// \return true if successful, false if it doesn't exist.
    fn remove(&mut self, name: &wstr) -> bool {
        if self.funcs.remove(name).is_some() {
            event::remove_function_handlers(name);
            return true;
        }
        false
    }

    /// Get the properties for a function, or nullptr if none.
    fn get_props(&self, name: &wstr) -> Option<FunctionPropertiesRef> {
        self.funcs.get(name).cloned()
    }

    /// \return true if we should allow autoloading a given function.
    fn allow_autoload(&self, name: &wstr) -> bool {
        // Prohibit autoloading if we have a non-autoload (explicit) function, or if the function is
        // tombstoned.
        let props = self.get_props(name);
        let has_explicit_func = props
            .map(|p| p.read().unwrap().is_autoload)
            .unwrap_or(false);
        let is_tombstoned = self.autoload_tombstones.contains(name);
        !has_explicit_func && !is_tombstoned
    }
}

/// The big set of all functions.
static FUNCTION_SET: Lazy<Mutex<FunctionSet>> = Lazy::new(|| Mutex::new(FunctionSet::new()));

// Safety: global lock.
unsafe impl Send for FunctionSet {}

/// \return a copy of some function props, in a new shared_ptr.
fn copy_props(props: &FunctionPropertiesRef) -> FunctionPropertiesRef {
    Arc::new(RwLock::new(props.read().unwrap().clone()))
}

/// Insert a list of all dynamically loaded functions into the specified list.
fn autoload_names(names: &mut HashSet<WString>, get_hidden: bool) {
    // TODO: justify this.
    let vars = EnvStack::principal();
    let Some(path_var) = vars.get_unless_empty(L!("fish_function_path")) else { return };

    let path_list = path_var.as_list();

    for ndir_str in path_list {
        let mut dir = DirIter::new(ndir_str);
        if !dir.valid() {
            continue;
        }
        while let Some(entry) = dir.next() {
            if !get_hidden && entry.name.as_char_slice().first() == Some(&'_') {
                continue;
            }

            // We need a ".fish" *suffix*, it can't be the entire name.
            if let Some(dotpos) = entry.name.as_char_slice().iter().rposition(|c| *c == '.') {
                if &entry.name[dotpos..] == L!(".fish") {
                    // Also ignore directories.
                    if !entry.is_dir() {
                        names.insert(entry.name[..dotpos].to_owned());
                    }
                }
            }
        }
    }
}

// \return the body of a function (everything after the header, up to but not including the 'end').
fn get_function_body_source(props: &FunctionProperties) -> WString {
    // We want to preserve comments that the AST attaches to the header (#5285).
    // Take everything from the end of the header to the 'end' keyword.
    let header_src = props.func_node.header.try_source_range();
    let end_kw_src = props.func_node.end.try_source_range();
    if let (Some(header_src), Some(end_kw_src)) = (header_src, end_kw_src) {
        let body_start = header_src.end();
        let body_end = end_kw_src.start();
        assert!(
            body_start <= body_end,
            "end keyword should come after header"
        );
        return props.parsed_source.as_ref().unwrap().src[body_start..body_end].to_owned();
    }
    WString::new()
}
