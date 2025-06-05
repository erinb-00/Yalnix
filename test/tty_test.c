#include <yuser.h>
#include <hardware.h>
#include <stdlib.h>

int main()
{
  int terminal = 0; // Use terminal 0
  char buffer[100];

  TtyPrintf(terminal, "*** Welcome to Terminal Test ***\n");
  TtyPrintf(terminal, "Your process ID is: %d\n", GetPid());

  // Testing TtyWrite with a different message
  char *test_message = "Testing TtyWrite function: Hello, World!\n";
  int written = TtyWrite(terminal, test_message, strlen(test_message));
  TtyPrintf(terminal, "TtyWrite wrote %d bytes\n\n", written);

  // Request favorite color
  TtyPrintf(terminal, "What's your favorite color? ");
  int read_bytes = TtyRead(terminal, buffer, sizeof(buffer) - 1);
  buffer[read_bytes] = '\0';
  TtyPrintf(terminal, "Ah, %s is a great choice!\n", buffer);
  // Multiple line echo test
  TtyPrintf(terminal, "Now, let's try echoing some lines. Type 2 lines below:\n");
  int lines_to_echo = 2;
  for (int i = 1; i <= lines_to_echo; i++)
  {
    TtyPrintf(terminal, "Input line %d: ", i);
    read_bytes = TtyRead(terminal, buffer, sizeof(buffer) - 1);
    buffer[read_bytes] = '\0';
    TtyPrintf(terminal, "You typed: %s\n", buffer);
  }

  TtyPrintf(terminal, "\nTest complete. Goodbye!\n");

  Exit(0);
}
