#include <yuser.h>

int main (){
    int c = Fork();
    if (c == 0){
        char* args[] = {"test/zero", NULL};
        Exec("test/zero", args);
    }
    
    int status;
    Wait(&status);
    TracePrintf(0, "Child process exited with status %d\n", status);
    return 0;
}
