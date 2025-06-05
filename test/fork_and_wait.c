#include <yuser.h>

int main(){
    int c = Fork();
    if (c == 0){
        Delay(5);
        Exit(0);
    }
    int status;
    Wait(&status);
    TracePrintf(0, "Child process exited with status %d\n", status);
    return 0;
}