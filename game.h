#ifndef GAME_H
#define GAME_H

#define BoardSize 15

extern int chessboard[BoardSize + 1][BoardSize + 1]; 
extern int whoseTurn; 

void initGame(void);
int judge(int playerTurn, int x, int y);
void printChessboard(void);
char* getMessage(const char* data);

#endif 
