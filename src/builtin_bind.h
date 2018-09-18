// Prototypes for executing builtin_bind function.
#ifndef FISH_BUILTIN_BIND_H
#define FISH_BUILTIN_BIND_H

#include "common.h"

#define DEFAULT_BIND_MODE L"default"

class parser_t;
struct io_streams_t;
struct bind_cmd_opts_t;

class builtin_bind_t {
public:
	int builtin_bind(parser_t &parser, io_streams_t &streams, wchar_t **argv);
private:
	bind_cmd_opts_t *opts;

	void list(const wchar_t *bind_mode, bool user, io_streams_t &streams);
	void key_names(bool all, io_streams_t &streams);
	void function_names(io_streams_t &streams);
	bool add(const wchar_t *seq, const wchar_t *const *cmds, size_t cmds_len,
             const wchar_t *mode, const wchar_t *sets_mode, bool terminfo, bool user,
								 io_streams_t &streams);
	bool erase(wchar_t **seq, bool all, const wchar_t *mode, bool use_terminfo, bool user,
								 io_streams_t &streams);
	bool get_terminfo_sequence(const wchar_t *seq, wcstring *out_seq, io_streams_t &streams);
	bool insert(int optind, int argc, wchar_t **argv,
								 io_streams_t &streams);
	void list_modes(io_streams_t &streams);
	bool list_one(const wcstring &seq, const wcstring &bind_mode, bool user, io_streams_t &streams);
	bool list_one(const wcstring &seq, const wcstring &bind_mode, bool user, bool preset, io_streams_t &streams);
};

inline int builtin_bind(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
	builtin_bind_t bind;
	return bind.builtin_bind(parser, streams, argv);
}

#endif
