#include <cstdint>

#include "cg_cgame.hpp"
#include "cmd_commands.hpp"
#include "con_console.hpp"
#include "devcon.hpp"
#include "devgui.hpp"
#include "dvar.hpp"
#include "font.hpp"
#include "gfx.hpp"
#include "in_input.hpp"
#include "pm_pmove.hpp"
#include "sys.hpp"

static uint64_t s_lastFrameTime;
static uint64_t s_deltaTime;

dvar_t* com_maxfps;

void Com_Quit_f() {
    Sys_NormalExit(3);
}

bool Com_Init() {
    com_maxfps = &Dvar_RegisterInt("com_maxfps", DVAR_FLAG_NONE, 165, 1, 1000);

    Cmd_Init();
    Cmd_AddCommand("quit", Com_Quit_f);
    Dvar_Init();
    Font_Init();
    PM_Init();
    CG_Init();
    R_Init();
    CL_Init();
    DevGui_Init();
    s_lastFrameTime = Sys_Milliseconds();
    s_deltaTime = s_lastFrameTime;
    return true;
}

bool Com_Frame() {
    uint64_t wait_msec = 1000 / (uint64_t)Dvar_GetInt(*com_maxfps);
    while (Sys_Milliseconds() - s_lastFrameTime < wait_msec);

    s_deltaTime = Sys_Milliseconds() - s_lastFrameTime;
    s_lastFrameTime = Sys_Milliseconds();

    IN_Frame();

    while (Sys_HandleEvent())
        ;

    DevCon_Frame();

    CG_Frame(s_deltaTime);
    CL_Frame();
    DevGui_Frame();
    for (size_t i = 0; i < MAX_LOCAL_CLIENTS; i++) {
        if(!CG_LocalClientIsActive(i))
            continue;

        if (DevGui_HasText(i))
            Con_ProcessInput(DevGui_TakeText(i), i);
    }
    if (DevCon_HasText())
        Con_ProcessInput(DevCon_TakeText());
    
    R_Frame();

    return true;
}

uint64_t Com_LastFrameTimeDelta() {
    return s_deltaTime;
}

uint64_t Com_LastFrameTime() {
    return s_lastFrameTime;
}

void Com_Shutdown() {
    DevGui_Shutdown();
    CL_Shutdown();
    CG_Shutdown();
    R_Shutdown();
    Font_Shutdown();
    Dvar_Shutdown();
    Cmd_Shutdown();

    Dvar_Unregister("com_maxfps");
    com_maxfps = NULL;
}
