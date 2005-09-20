/** \file env_universl.h
	Universal variable client library
*/

#ifndef ENV_UNIVERSAL_HH
#define ENV_UNIVERSAL_HH

#include "env_universal_common.h"

/**
   Data about the universal variable server.
*/
extern connection_t env_universal_server;

/**
   Initialize the envuni library
*/
void env_universal_init();
/*
  Free memory used by envuni
*/
void env_universal_destroy();

/**
   Get the value of a universal variable
*/
wchar_t *env_universal_get( const wchar_t *name );
/**
   Set the value of a universal variable
*/
void env_universal_set( const wchar_t *name, const wchar_t *val );
/**
   Erase a universal variable
*/
void env_universal_remove( const wchar_t *name );

int env_universal_read_all();

#endif
