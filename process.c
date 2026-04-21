#include "headers.h"

int remainingtime;
volatile sig_atomic_t resumedFlag = 0;

void handleCont(int signum)
{
    resumedFlag = 1;
}

int main(int argc, char *argv[])
{
    initClk();
    signal(SIGCONT, handleCont);

    remainingtime = atoi(argv[1]);

    int lastClk = getClk();

    while (remainingtime > 0)
    {
        /* If we just resumed from SIGSTOP, re-sync the clock
           without counting the tick we were stopped during */
        if (resumedFlag)
        {
            resumedFlag = 0;
            lastClk = getClk();
        }

        /* Busy-wait until the clock advances by exactly one tick */
        int currentClk;
        while ((currentClk = getClk()) == lastClk)
        {
            if (resumedFlag)
            {
                resumedFlag = 0;
                lastClk = getClk();
            }
        }

        /* One full tick has passed — count it */
        lastClk = currentClk;
        remainingtime--;
    }

    destroyClk(false);
    return 0;
}