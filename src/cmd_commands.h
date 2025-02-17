#pragma once

#include <stdbool.h>

#include "com_defs.h"

#define CMD_MAX_COMMANDS 16
#define CMD_MAX_ARGS     4 

A_EXTERN_C void        Cmd_Init();
A_EXTERN_C bool        Cmd_AddCommand   (const char* cmdName, void(*fn)(void));
A_EXTERN_C bool		   Cmd_CommandExists(const char* cmdName);
A_EXTERN_C bool        Cmd_FindCommand  (const char* cmdName, A_OUT void(**fn)(void));
A_EXTERN_C void        Cmd_RemoveCommand(const char* cmdName);
A_EXTERN_C void        Cmd_ClearCommands();
A_EXTERN_C int         Cmd_Argc();
A_EXTERN_C const char* Cmd_Argv         (int i);
A_EXTERN_C bool        Cmd_TakeInput    (const char* input);
A_EXTERN_C void        Cmd_Shutdown();
