#include "snake.h"
#include <stdlib.h>
static struct Snake snake;
static int food_x, food_y;
void init_game(void) {
  snake.x = WIDTH/2;
  snake.y = HEIGHT/2;
  snake.len = 1;
  food_x = rand() % WIDTH;
  food_y = rand() % HEIGHT;
}
