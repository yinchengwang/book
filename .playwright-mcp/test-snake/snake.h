#ifndef SNAKE_H
#define SNAKE_H
#define WIDTH 40
#define HEIGHT 20
struct Snake { int x, y; int len; };
void init_game(void);
void update(void);
void render(void);
#endif
