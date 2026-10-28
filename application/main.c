#include "app.h"
#include "board.h"

int main(void)
{
    if (!Board_Init())
    {
        while (1)
        {
            Board_LedToggle(1U);
        }
    }
    APP_Init();
    APP_Run();

    return 0;
}
