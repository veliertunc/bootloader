#include "bl_state.h"

static BL_State_t g_CurrentState;

static BL_State_t g_PreviousState;

void BL_StateInit(void)
{
    g_CurrentState =
        BL_STATE_IDLE;

    g_PreviousState =
        BL_STATE_IDLE;
}

void BL_StateSet(
    BL_State_t state)
{
    if(state == g_CurrentState)
    {
        return;
    }

    g_PreviousState =
        g_CurrentState;

    g_CurrentState =
        state;
}

BL_State_t
BL_StateGet(void)
{
    return
        g_CurrentState;
}

BL_State_t
BL_StateGetPrevious(void)
{
    return
        g_PreviousState;
}

const char *
BL_StateToString(
    BL_State_t state)
{
    switch(state)
    {
        case BL_STATE_IDLE:
            return "IDLE";

        case BL_STATE_WAIT_CONNECTION:
            return "WAIT_CONNECTION";

        case BL_STATE_ERASE:
            return "ERASE";

        case BL_STATE_PROGRAM:
            return "PROGRAM";

        case BL_STATE_VERIFY:
            return "VERIFY";

        case BL_STATE_COMPLETE:
            return "COMPLETE";

        case BL_STATE_ERROR:
            return "ERROR";

        default:
            return "UNKNOWN";
    }
}

