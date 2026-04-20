#include "headers.h"

int remainingtime;
int lastClk;
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
    lastClk = getClk();

    while (remainingtime > 0)
    {
        if (resumedFlag)
        {
            lastClk = getClk();
            resumedFlag = 0;
        }

        while (getClk() == lastClk)
        {
            if (resumedFlag)
            {
                lastClk = getClk();
                resumedFlag = 0;
            }
        }

        lastClk = getClk();
        remainingtime--;
    }

    kill(getppid(), SIGUSR1);
    destroyClk(false);
    return 0;
}