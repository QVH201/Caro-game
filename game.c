#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"

#define BUF_SIZE 1024

int chessboard[BoardSize + 1][BoardSize + 1] = {0};
int whoseTurn = 0;
int game_over = 0; 

void initGame(void) {
    int i, j;
    for (i = 0; i <= BoardSize; i++) {
        for (j = 0; j <= BoardSize; j++) {
            chessboard[i][j] = 0;
        }
    }
}

int judge(int playerTurn, int x, int y) {
    const int step[4][2] = {{-1, 0}, {0, -1}, {1, 1}, {1, -1}}; // 4 hướng: dọc, ngang, chéo xuống phải, chéo xuống trái
    for (int i = 0; i < 4; i++) {
        int count = 1; // Đếm quân liên tiếp
        for (int dir = -1; dir <= 1; dir += 2) { // Kiểm tra cả 2 phía
            for (int k = 1; k <= 4; k++) {
                int nx = x + k * step[i][0] * dir;
                int ny = y + k * step[i][1] * dir;
                if (nx >= 1 && nx <= BoardSize && ny >= 1 && ny <= BoardSize &&
                    chessboard[nx][ny] == playerTurn) {
                    count++;
                } else {
                    break;
                }
            }
        }
        if (count >= 5) {
            return 1; // Có 5 quân liên tiếp
        }
    }
    return 0; // Không có 5 quân liên tiếp
}

void printChessboard(void) {
    int i, j;
    for (i = 0; i <= BoardSize; i++) {
        for (j = 0; j <= BoardSize; j++) {
            if (i == 0)
                printf("%3d", j);
            else if (j == 0)
                printf("%3d", i);
            else if (chessboard[i][j] == 1)
                printf("  X");
            else if (chessboard[i][j] == 2)
                printf("  O");
            else
                printf("  *");
        }
        printf("\n");
    }
}

char* getMessage(const char* data) {
    // Tạo một thông điệp với độ dài lớn
    char* message = (char*)malloc(1024 * sizeof(char));
    char fromUser[50] = "";
    int moveX = -1, moveY = -1;

    // Parse dữ liệu từ `data`, bao gồm người gửi và nước cờ
    sscanf(data, "{\"from_user\": \"%49[^\"]\", \"moveX\": \"%d\", \"moveY\": \"%d\"}", fromUser, &moveX, &moveY);

    // Xác định lượt đi của người chơi
    if (strcmp(fromUser, "player1") == 0) {
        whoseTurn = 1;
    } else if (strcmp(fromUser, "player2") == 0) {
        whoseTurn = 2;
    }

    // Kiểm tra nước cờ có hợp lệ không
    if (chessboard[moveX][moveY] != 0) {
        // Nếu ô đã có quân cờ, nước cờ không hợp lệ
        strcpy(message, "{\"type\": \"invalid_move\", \"data\": {}}");
    } else {
        // Đặt quân vào ô
        chessboard[moveX][moveY] = whoseTurn;

        // Kiểm tra xem người chơi có chiến thắng không
        int checkMove = judge(whoseTurn, moveX, moveY);
        if (checkMove == 1) { // Có người thắng
            if (whoseTurn == 1) {
                strcpy(message, "{\"type\": \"win_move\", \"data\": {\"player\": \"player1\"}}");
            } else {
                strcpy(message, "{\"type\": \"win_move\", \"data\": {\"player\": \"player2\"}}");
            }
            printf("Game over! Winner: %s\n", whoseTurn == 1 ? "player1" : "player2");
        } else {
            // Kiểm tra tình huống hòa (không có ô trống nào còn lại)
            int draw = 1;
            for (int i = 1; i <= BoardSize; i++) {
                for (int j = 1; j <= BoardSize; j++) {
                    if (chessboard[i][j] == 0) {  // Nếu có ô trống thì không hòa
                        draw = 0;
                        break;
                    }
                }
                if (!draw) break;
            }

            // Nếu không còn ô trống và không có ai thắng, trận đấu là hòa
            if (draw) {
                strcpy(message, "{\"type\": \"draw_game\", \"data\": {}}");
                printf("Game over! It's a draw.\n");
            } else {
                // Nếu không có người thắng, thông báo tiếp tục quá trình nước cờ
                sprintf(message, "{\"type\": \"process_move\", \"data\": {\"moveX\": \"%d\", \"moveY\": \"%d\"}}", moveX, moveY);
            }
        }
    }
    
    return message;
}