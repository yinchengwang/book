#include <stdio.h>
#include "snake.h"
int main() {
  init_game();
  while(1) { update(); render(); }
  return 0;
}
