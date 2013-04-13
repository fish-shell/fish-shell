/** \file env_universal.h
  Universal variable client library.
*/

#ifndef ENV_UNIVERSAL_H
#define ENV_UNIVERSAL_H

#include <wchar.h>

#include "env_universal_common.h"

/**
   Data about the universal variable server.
*/
extern connection_t env_universal_server;

/**
   Initialize the envuni library
*/
void env_universal_init(wchar_t * p,
                        wchar_t *u,
                        void (*sf)(),
                        void (*cb)(fish_message_type_t type, const wchar_t *name, const wchar_t *val));
/**
  Free memory used by envuni
*/
void env_universal_destroy();

/**
   Get the value of a universal variable
*/
const wchar_t *env_universal_get(const wcstring &name);

/**
   Get the export flag of the variable with the specified
   name. Returns 0 if the variable doesn't exist.
*/
bool env_universal_get_export(const wcstring &name);

/**
   Set the value of a universal variable
*/
void env_universal_set(const wcstring &name, const wcstring &val, bool exportv);
/**
   Erase a universal variable

   \return zero if the variable existed, and non-zero if the variable did not exist
*/
int env_universal_remove(const wchar_t *name);

/**
   Read all available messages from the server.
*/
int env_universal_read_all();

/**
   Get the names of all universal variables

   \param l the list to insert the names into
   \param show_exported whether exported variables should be shown
   \param show_unexported whether unexported variables should be shown
*/
void env_universal_get_names(wcstring_list_t &list,
                             bool show_exported,
                             bool show_unexported);

/**
   Synchronize with fishd
*/
void env_universal_barrier();

#endif
