// Functions for storing and retrieving function information. These functions also take care of
// autoloading functions in the $fish_function_path. Actual function evaluation is taken care of by
// the parser and to some degree the builtin handling library.

use crate::ast::{self, Node};
use crate::common::{assert_sync, escape, valid_func_name, FilenameRef};
use crate::env::{EnvStack, Environment};
use crate::event::{self, EventDescription};
use crate::ffi::{self, parser_t, Repin};
use crate::global_safety::RelaxedAtomicBool;
use crate::parse_tree::{NodeRef, ParsedSourceRefFFI};
use crate::parser_keywords::parser_keywords_is_reserved;
use crate::wchar::prelude::*;
use crate::wchar_ffi::wcstring_list_ffi_t;
use crate::wchar_ffi::{AsWstr, WCharFromFFI, WCharToFFI};
use crate::wutil::{dir_iter::DirIter, gettext::wgettext_expr, sprintf};
use cxx::{CxxWString, UniquePtr};
use once_cell::sync::Lazy;
use std::collections::{HashMap, HashSet};
use std::pin::Pin;
use std::sync::{Arc, Mutex};

#[derive(Clone)]
pub struct FunctionProperties {
    /// Reference to the node, along with the parsed source.
    pub func_node: NodeRef<ast::BlockStatement>,

    /// List of all named arguments for this function.
    pub named_arguments: Vec<WString>,

    /// Description of the function.
    pub description: WString,

    /// Mapping of all variables that were inherited from the function definition scope to their
    /// values, as (key, values) pairs.
    pub inherit_vars: Box<[(WString, Vec<WString>)]>,

    /// Set to true if invoking this function shadows the variables of the underlying function.
    pub shadow_scope: bool,

    /// Whether the function was autoloaded.
    /// This is the only field which is mutated after the properties are created.
    pub is_autoload: RelaxedAtomicBool,

    /// The file from which the function was created, or None if not from a file.
    pub definition_file: Option<FilenameRef>,

    /// Whether the function was copied.
    pub is_copy: bool,

    /// The file from which the function was copied, or None if not from a file.
    pub copy_definition_file: Option<FilenameRef>,

    /// The line number where the specified function was copied.
    pub copy_definition_lineno: i32,
}

/// FunctionProperties are safe to share between threads.
const _: () = assert_sync::<FunctionProperties>();

/// Type wrapping up the set of all functions.
/// There's only one of these; it's managed by a lock.
struct FunctionSet {
    /// The map of all functions by name.
    funcs: HashMap<WString, Arc<FunctionProperties>>,

    /// Tombstones for functions that should no longer be autoloaded.
    autoload_tombstones: HashSet<WString>,

    /// The autoloader for our functions.
    autoloader: cxx::UniquePtr<ffi::autoload_t>,
}

impl FunctionSet {
    /// Remove a function.
    /// \return true if successful, false if it doesn't exist.
    fn remove(&mut self, name: &wstr) -> bool {
        if self.funcs.remove(name).is_some() {
            event::remove_function_handlers(name);
            true
        } else {
            false
        }
    }

    /// Get the properties for a function, or None if none.
    fn get_props(&self, name: &wstr) -> Option<Arc<FunctionProperties>> {
        self.funcs.get(name).cloned()
    }

    /// \return true if we should allow autoloading a given function.
    fn allow_autoload(&self, name: &wstr) -> bool {
        // Prohibit autoloading if we have a non-autoload (explicit) function, or if the function is
        // tombstoned.
        let props = self.get_props(name);
        let has_explicit_func =
            props.map_or(false, |p: Arc<FunctionProperties>| !p.is_autoload.load());
        let tombstoned = self.autoload_tombstones.contains(name);
        !has_explicit_func && !tombstoned
    }
}

/// The big set of all functions.
static FUNCTION_SET: Lazy<Mutex<FunctionSet>> = Lazy::new(|| {
    Mutex::new(FunctionSet {
        funcs: HashMap::new(),
        autoload_tombstones: HashSet::new(),
        autoloader: ffi::make_autoload_ffi(L!("fish_function_path").to_ffi()),
    })
});

/// Necessary until autoloader has been ported to Rust.
unsafe impl Send for FunctionSet {}

/// Make sure that if the specified function is a dynamically loaded function, it has been fully
/// loaded. Note this executes fish script code.
fn load(name: &wstr, parser: &mut parser_t) -> bool {
    parser.assert_can_execute();
    let mut path_to_autoload: Option<WString> = None;
    // Note we can't autoload while holding the funcset lock.
    // Lock around a local region.
    {
        let mut funcset: std::sync::MutexGuard<FunctionSet> = FUNCTION_SET.lock().unwrap();
        if funcset.allow_autoload(name) {
            let path = funcset
                .autoloader
                .as_mut()
                .unwrap()
                .resolve_command_ffi(&name.to_ffi() /* Environment::globals() */)
                .from_ffi();
            if !path.is_empty() {
                path_to_autoload = Some(path);
            }
        }
    }

    // Release the lock and perform any autoload, then reacquire the lock and clean up.
    if let Some(path_to_autoload) = path_to_autoload.as_ref() {
        // Crucially, the lock is acquired after perform_autoload().
        ffi::perform_autoload_ffi(&path_to_autoload.to_ffi(), parser.pin());
        FUNCTION_SET
            .lock()
            .unwrap()
            .autoloader
            .as_mut()
            .unwrap()
            .mark_autoload_finished(&name.to_ffi());
    }
    path_to_autoload.is_some()
}

/// Insert a list of all dynamically loaded functions into the specified list.
fn autoload_names(names: &mut HashSet<WString>, get_hidden: bool) {
    // TODO: justify this.
    let vars = EnvStack::principal();
    let Some(path_var) = vars.get_unless_empty(L!("fish_function_path")) else {
        return;
    };
    let path_list = path_var.as_list();
    for ndir_str in path_list {
        let Ok(mut dir) = DirIter::new(ndir_str) else {
            continue;
        };
        while let Some(entry) = dir.next() {
            let Ok(entry) = entry else {
                continue;
            };
            let func: &WString = &entry.name;
            if !get_hidden && func.char_at(0) == '_' {
                continue;
            }
            let suffix: Option<usize> = func.chars().rposition(|x| x == '.');
            // We need a ".fish" *suffix*, it can't be the entire name.
            if let Some(suffix) = suffix {
                if suffix > 0 && entry.name.slice_from(suffix) == ".fish" {
                    // Also ignore directories.
                    if !entry.is_dir() {
                        let name = entry.name.slice_to(suffix).to_owned();
                        names.insert(name);
                    }
                }
            }
        }
    }
}

/// Add a function. This may mutate \p props to set is_autoload.
pub fn add(name: WString, props: Arc<FunctionProperties>) {
    let mut funcset = FUNCTION_SET.lock().unwrap();

    // Historical check. TODO: rationalize this.
    if name.is_empty() {
        return;
    }

    // Remove the old function.
    funcset.remove(&name);

    // Check if this is a function that we are autoloading.
    props
        .is_autoload
        .store(funcset.autoloader.autoload_in_progress(&name.to_ffi()));

    // Create and store a new function.
    let existing = funcset.funcs.insert(name, props);
    assert!(
        existing.is_none(),
        "Function should not already be present in the table"
    );
}

/// \return the properties for a function, or None. This does not trigger autoloading.
pub fn get_props(name: &wstr) -> Option<Arc<FunctionProperties>> {
    if parser_keywords_is_reserved(name) {
        None
    } else {
        FUNCTION_SET.lock().unwrap().get_props(name)
    }
}

/// \return the properties for a function, or None, perhaps triggering autoloading.
pub fn get_props_autoload(name: &wstr, parser: &mut parser_t) -> Option<Arc<FunctionProperties>> {
    parser.assert_can_execute();
    if parser_keywords_is_reserved(name) {
        return None;
    }
    load(name, parser);
    get_props(name)
}

/// Returns true if the function named \p cmd exists.
/// This may autoload.
pub fn exists(cmd: &wstr, parser: &mut parser_t) -> bool {
    parser.assert_can_execute();
    if !valid_func_name(cmd) {
        return false;
    }
    get_props_autoload(cmd, parser).is_some()
}

/// Returns true if the function \p cmd either is loaded, or exists on disk in an autoload
/// directory.
pub fn exists_no_autoload(cmd: &wstr) -> bool {
    if !valid_func_name(cmd) {
        return false;
    }
    if parser_keywords_is_reserved(cmd) {
        return false;
    }
    let mut funcset = FUNCTION_SET.lock().unwrap();
    // Check if we either have the function, or it could be autoloaded.
    funcset.get_props(cmd).is_some()
        || funcset
            .autoloader
            .as_mut()
            .unwrap()
            .can_autoload(&cmd.to_ffi())
}

/// Remove the function with the specified name.
pub fn remove(name: &wstr) {
    let mut funcset = FUNCTION_SET.lock().unwrap();
    funcset.remove(name);
    // Prevent (re-)autoloading this function.
    funcset.autoload_tombstones.insert(name.to_owned());
}

// \return the body of a function (everything after the header, up to but not including the 'end').
fn get_function_body_source(props: &FunctionProperties) -> &wstr {
    // We want to preserve comments that the AST attaches to the header (#5285).
    // Take everything from the end of the header to the 'end' keyword.
    let Some(header_source) = props.func_node.header.try_source_range() else {
        return L!("");
    };
    let Some(end_kw_source) = props.func_node.end.try_source_range() else {
        return L!("");
    };
    let body_start = header_source.start as usize + header_source.length as usize;
    let body_end = end_kw_source.start as usize;
    assert!(
        body_start <= body_end,
        "end keyword should come after header"
    );
    props
        .func_node
        .parsed_source()
        .src
        .slice_to(body_end)
        .slice_from(body_start)
}

/// Sets the description of the function with the name \c name.
/// This triggers autoloading.
pub fn set_desc(name: &wstr, desc: WString, parser: &mut parser_t) {
    parser.assert_can_execute();
    load(name, parser);
    let mut funcset = FUNCTION_SET.lock().unwrap();
    if let Some(props) = funcset.funcs.get(name) {
        // Note the description is immutable, as it may be accessed on another thread, so we copy
        // the properties to modify it.
        let mut new_props = props.as_ref().clone();
        new_props.description = desc;
        funcset.funcs.insert(name.to_owned(), Arc::new(new_props));
    }
}

/// Creates a new function using the same definition as the specified function. Returns true if copy
/// is successful.
pub fn copy(name: &wstr, new_name: WString, parser: &parser_t) -> bool {
    let filename = parser.current_filename_ffi().from_ffi();
    let lineno = parser.get_lineno();

    let mut funcset = FUNCTION_SET.lock().unwrap();
    let Some(props) = funcset.get_props(name) else {
        // No such function.
        return false;
    };
    // Copy the function's props.
    let mut new_props = props.as_ref().clone();
    new_props.is_autoload.store(false);
    new_props.is_copy = true;
    new_props.copy_definition_file = Some(Arc::new(filename));
    new_props.copy_definition_lineno = lineno.into();

    // Note this will NOT overwrite an existing function with the new name.
    // TODO: rationalize if this behavior is desired.
    funcset.funcs.entry(new_name).or_insert(Arc::new(new_props));
    return true;
}

/// Returns all function names.
///
/// \param get_hidden whether to include hidden functions, i.e. ones starting with an underscore.
pub fn get_names(get_hidden: bool) -> Vec<WString> {
    let mut names = HashSet::<WString>::new();
    let funcset = FUNCTION_SET.lock().unwrap();
    autoload_names(&mut names, get_hidden);
    for name in funcset.funcs.keys() {
        // Maybe skip hidden.
        if !get_hidden && (name.is_empty() || name.char_at(0) == '_') {
            continue;
        }
        names.insert(name.clone());
    }
    names.into_iter().collect()
}

/// Observes that fish_function_path has changed.
pub fn invalidate_path() {
    // Remove all autoloaded functions and update the autoload path.
    let mut funcset = FUNCTION_SET.lock().unwrap();
    funcset.funcs.retain(|_, props| !props.is_autoload.load());
    funcset.autoloader.as_mut().unwrap().clear();
}

impl FunctionProperties {
    /// Return the description, localized via wgettext.
    pub fn localized_description(&self) -> &'static wstr {
        if self.description.is_empty() {
            L!("")
        } else {
            wgettext_expr!(&self.description)
        }
    }

    /// Return true if this function is a copy.
    pub fn is_copy(&self) -> bool {
        self.is_copy
    }

    /// Return a reference to the function's definition file, or None if it was defined interactively or copied.
    pub fn definition_file(&self) -> Option<&wstr> {
        self.definition_file.as_ref().map(|f| f.as_utfstr())
    }

    /// Return a reference to the vars that this function has inherited from its definition scope.
    pub fn inherit_vars(&self) -> &[(WString, Vec<WString>)] {
        &self.inherit_vars
    }

    /// If this function is a copy, return a reference to the original definition file, or None if it was defined interactively or copied.
    pub fn copy_definition_file(&self) -> Option<&wstr> {
        self.copy_definition_file.as_ref().map(|f| f.as_utfstr())
    }

    /// Return the 1-based line number of the function's definition.
    pub fn definition_lineno(&self) -> i32 {
        // Return one plus the number of newlines at offsets less than the start of our function's
        // statement (which includes the header).
        // TODO: merge with line_offset_of_character_at_offset?
        let Some(source_range) = self.func_node.try_source_range() else {
            panic!("Function has no source range");
        };
        let func_start = source_range.start as usize;
        let source = &self.func_node.parsed_source().src;
        assert!(
            func_start <= source.char_count(),
            "function start out of bounds"
        );
        1 + source
            .slice_to(func_start)
            .chars()
            .filter(|&c| c == '\n')
            .count() as i32
    }

    /// If this function is a copy, return the original line number, else 0.
    pub fn copy_definition_lineno(&self) -> i32 {
        self.copy_definition_lineno
    }

    /// Return a definition of the function, annotated with properties like event handlers and wrap
    /// targets. This is to support the 'functions' builtin.
    /// Note callers must provide the function name, since the function does not know its own name.
    pub fn annotated_definition(&self, name: &wstr) -> WString {
        let mut out = WString::new();
        let desc = self.localized_description();
        let def = get_function_body_source(self);
        let handlers = event::get_function_handlers(name);

        out.push_str("function ");
        // Typically we prefer to specify the function name first, e.g. "function foo --description bar"
        // But if the function name starts with a -, we'll need to output it after all the options.
        let defer_function_name = name.char_at(0) == '-';
        if !defer_function_name {
            out.push_utfstr(&escape(name));
        }

        // Output wrap targets.
        for wrap in ffi::complete_get_wrap_targets_ffi(&name.to_ffi()).from_ffi() {
            out.push_str(" --wraps=");
            out.push_utfstr(&escape(&wrap));
        }

        if !desc.is_empty() {
            out.push_str(" --description ");
            out.push_utfstr(&escape(desc));
        }

        if !self.shadow_scope {
            out.push_str(" --no-scope-shadowing");
        }

        for handler in handlers {
            let d = &handler.desc;
            match d {
                EventDescription::Signal { signal } => {
                    sprintf!(=> &mut out, " --on-signal %ls", signal.name());
                }
                EventDescription::Variable { name } => {
                    sprintf!(=> &mut out, " --on-variable %ls", name);
                }
                EventDescription::ProcessExit { pid } => {
                    sprintf!(=> &mut out, " --on-process-exit %d", pid);
                }
                EventDescription::JobExit { pid, .. } => {
                    sprintf!(=> &mut out, " --on-job-exit %d", pid);
                }
                EventDescription::CallerExit { .. } => {
                    out.push_str(" --on-job-exit caller");
                }
                EventDescription::Generic { param } => {
                    sprintf!(=> &mut out, " --on-event %ls", param);
                }
                EventDescription::Any => {
                    panic!("Unexpected event handler type");
                }
            };
        }

        let named = &self.named_arguments;
        if !named.is_empty() {
            out.push_str(" --argument");
            for name in named {
                // TODO: should these names be escaped?
                sprintf!(=> &mut out, " %ls", name);
            }
        }

        // Output the function name if we deferred it.
        if defer_function_name {
            out.push_str(" -- ");
            out.push_utfstr(&escape(name));
        }

        // Output any inherited variables as `set -l` lines.
        for (name, values) in self.inherit_vars.iter() {
            // We don't know what indentation style the function uses,
            // so we do what fish_indent would.
            sprintf!(=> &mut out, "\n    set -l %ls", name);
            for arg in values {
                out.push(' ');
                out.push_utfstr(&escape(arg));
            }
        }
        out.push('\n');
        out.push_utfstr(def);

        // Append a newline before the 'end', unless there already is one there.
        if !def.ends_with('\n') {
            out.push('\n');
        }
        out.push_str("end\n");
        out
    }
}

pub struct FunctionPropertiesRefFFI(pub Arc<FunctionProperties>);

impl FunctionPropertiesRefFFI {
    fn definition_file(&self) -> UniquePtr<CxxWString> {
        if let Some(file) = self.0.definition_file() {
            file.to_ffi()
        } else {
            UniquePtr::null()
        }
    }

    fn definition_lineno(&self) -> i32 {
        self.0.definition_lineno()
    }

    fn copy_definition_lineno(&self) -> i32 {
        self.0.copy_definition_lineno()
    }

    fn shadow_scope(&self) -> bool {
        self.0.shadow_scope
    }

    fn named_arguments(&self) -> UniquePtr<wcstring_list_ffi_t> {
        self.0.named_arguments.to_ffi()
    }

    fn get_description(&self) -> UniquePtr<CxxWString> {
        self.0.description.to_ffi()
    }

    fn annotated_definition(&self, name: &CxxWString) -> UniquePtr<CxxWString> {
        self.0.annotated_definition(name.as_wstr()).to_ffi()
    }

    fn is_autoload(&self) -> bool {
        self.0.is_autoload.load()
    }

    fn is_copy(&self) -> bool {
        self.0.is_copy
    }

    fn get_block_statement_node_ffi(&self) -> *const u8 {
        let stmt: &ast::BlockStatement = &self.0.func_node;
        stmt as *const ast::BlockStatement as *const u8
    }

    fn parsed_source_ffi(&self) -> *mut u8 {
        let source = self.0.func_node.parsed_source_ref();
        let res = Box::new(ParsedSourceRefFFI(Some(source)));
        Box::into_raw(res) as *mut u8
    }

    fn copy_definition_file_ffi(&self) -> UniquePtr<CxxWString> {
        if let Some(file) = self.0.copy_definition_file() {
            file.to_ffi()
        } else {
            UniquePtr::null()
        }
    }
}

#[allow(clippy::boxed_local)]
fn function_add_ffi(name: &CxxWString, props: Box<FunctionPropertiesRefFFI>) {
    add(name.from_ffi(), props.0);
}

fn function_remove_ffi(name: &CxxWString) {
    remove(name.as_wstr());
}

fn function_get_props_ffi(name: &CxxWString) -> *mut FunctionPropertiesRefFFI {
    let props = get_props(name.as_wstr());
    if let Some(props) = props {
        Box::into_raw(Box::new(FunctionPropertiesRefFFI(props)))
    } else {
        std::ptr::null_mut()
    }
}

fn function_get_props_autoload_ffi(
    name: &CxxWString,
    parser: Pin<&mut parser_t>,
) -> *mut FunctionPropertiesRefFFI {
    let props = get_props_autoload(name.as_wstr(), parser.unpin());
    if let Some(props) = props {
        Box::into_raw(Box::new(FunctionPropertiesRefFFI(props)))
    } else {
        std::ptr::null_mut()
    }
}

fn function_load_ffi(name: &CxxWString, parser: Pin<&mut parser_t>) -> bool {
    load(name.as_wstr(), parser.unpin())
}

fn function_set_desc_ffi(name: &CxxWString, desc: &CxxWString, parser: Pin<&mut parser_t>) {
    set_desc(name.as_wstr(), desc.from_ffi(), parser.unpin());
}

fn function_exists_ffi(cmd: &CxxWString, parser: Pin<&mut parser_t>) -> bool {
    exists(cmd.as_wstr(), parser.unpin())
}

fn function_exists_no_autoload_ffi(cmd: &CxxWString) -> bool {
    exists_no_autoload(cmd.as_wstr())
}

fn function_get_names_ffi(get_hidden: bool, mut out: Pin<&mut wcstring_list_ffi_t>) {
    let names = get_names(get_hidden);
    for name in names {
        out.as_mut().push(name.to_ffi());
    }
}

fn function_copy_ffi(name: &CxxWString, new_name: &CxxWString, parser: Pin<&mut parser_t>) -> bool {
    copy(name.as_wstr(), new_name.from_ffi(), parser.unpin())
}

#[cxx::bridge]
mod function_ffi {
    extern "C++" {
        include!("ast.h");
        include!("parse_tree.h");
        include!("parser.h");
        include!("wutil.h");
        type parser_t = crate::ffi::parser_t;
        type wcstring_list_ffi_t = crate::ffi::wcstring_list_ffi_t;
    }

    extern "Rust" {
        #[cxx_name = "function_properties_t"]
        type FunctionPropertiesRefFFI;

        fn definition_file(&self) -> UniquePtr<CxxWString>;
        fn definition_lineno(&self) -> i32;
        fn copy_definition_lineno(&self) -> i32;
        fn shadow_scope(&self) -> bool;
        fn named_arguments(&self) -> UniquePtr<wcstring_list_ffi_t>;
        fn get_description(&self) -> UniquePtr<CxxWString>;
        fn annotated_definition(&self, name: &CxxWString) -> UniquePtr<CxxWString>;
        fn is_autoload(&self) -> bool;
        fn is_copy(&self) -> bool;

        #[cxx_name = "copy_definition_file"]
        fn copy_definition_file_ffi(&self) -> UniquePtr<CxxWString>;

        /// Returns unowned pointer to BlockStatement, cast to a u8.
        #[cxx_name = "get_block_statement_node"]
        fn get_block_statement_node_ffi(&self) -> *const u8;

        /// Returns rust::Box<ParsedSourceRefFFI>::into_raw(), cast to a u8.
        #[cxx_name = "parsed_source"]
        fn parsed_source_ffi(self: &FunctionPropertiesRefFFI) -> *mut u8;

        #[cxx_name = "function_add"]
        fn function_add_ffi(name: &CxxWString, props: Box<FunctionPropertiesRefFFI>);

        #[cxx_name = "function_remove"]
        fn function_remove_ffi(name: &CxxWString);

        /// Returns a Box<FunctionPropertiesRefFFI>::into_raw(), or nullptr if None.
        #[cxx_name = "function_get_props_raw"]
        fn function_get_props_ffi(name: &CxxWString) -> *mut FunctionPropertiesRefFFI;

        /// Returns a Box<FunctionPropertiesRefFFI>::into_raw(), or nullptr if None.
        #[cxx_name = "function_get_props_autoload_raw"]
        fn function_get_props_autoload_ffi(
            name: &CxxWString,
            parser: Pin<&mut parser_t>,
        ) -> *mut FunctionPropertiesRefFFI;

        #[cxx_name = "function_load"]
        fn function_load_ffi(name: &CxxWString, parser: Pin<&mut parser_t>) -> bool;

        #[cxx_name = "function_set_desc"]
        fn function_set_desc_ffi(name: &CxxWString, desc: &CxxWString, parser: Pin<&mut parser_t>);

        #[cxx_name = "function_exists"]
        fn function_exists_ffi(cmd: &CxxWString, parser: Pin<&mut parser_t>) -> bool;

        #[cxx_name = "function_exists_no_autoload"]
        fn function_exists_no_autoload_ffi(cmd: &CxxWString) -> bool;

        #[cxx_name = "function_get_names"]
        fn function_get_names_ffi(get_hidden: bool, out: Pin<&mut wcstring_list_ffi_t>);

        #[cxx_name = "function_copy"]
        fn function_copy_ffi(
            name: &CxxWString,
            new_name: &CxxWString,
            parser: Pin<&mut parser_t>,
        ) -> bool;

        #[cxx_name = "function_invalidate_path"]
        fn invalidate_path();
    }
}

unsafe impl cxx::ExternType for FunctionPropertiesRefFFI {
    type Id = cxx::type_id!("function_properties_t");
    type Kind = cxx::kind::Opaque;
}
