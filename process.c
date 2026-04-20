#include "headers.h"

/* Modify this file as needed*/
int remainingtime;


int main(int argc, char *argv[])
{
    initClk();

    int remainingtime = atoi(argv[1]);

    int lastClk = getClk();

    while (remainingtime > 0)
    {
        sleep(1);
        remainingtime--;
    }

    // tell scheduler I finished
    kill(getppid(), SIGUSR1);

    destroyClk(false);
    return 0;
}
