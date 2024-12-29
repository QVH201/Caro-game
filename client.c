#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/select.h>

#define PORT 8080
#define BUF_SIZE 1024
#define BOARD_SIZE 8

int game_active = 0;
char player[50];
char challenger[50];
char opponent[50];
char moves[2048];
int waiting_for_input=0;
int your_turn = 0;
int chessboard[BOARD_SIZE + 1][BOARD_SIZE + 1] = {0};
int move_number = 1;

// Function prototypes
void process_server_response_expected(int socket, char *buffer, const char *username);
void handle_reguler_comand(char *command, int *logged_in, char *username, char *password, char buffer[1024], int socket, char target_user[50]);
void handle_exception_command(char *command, char response_message[1024], int socket);

void log_game(char *player1, char *player2, char *ip1, char *ip2, char *moves, const char *result) {
    FILE *logfile = fopen("game_log.txt", "a");
    if (logfile == NULL) {
        printf("Error opening log file!\n");
        return;
    }

    time_t now;
    time(&now);
    struct tm *start_tm = localtime(&now);

    fprintf(logfile, "Game started: %s", asctime(start_tm)); // Ghi thời gian bắt đầu
    fprintf(logfile, "Player1: %s (IP: %s)\n", player1, ip1);
    fprintf(logfile, "Player2: %s (IP: %s)\n", player2, ip2);
    fprintf(logfile, "Moves:\n%s\n", moves); // Ghi danh sách các nước đi
    fprintf(logfile, "Result: %s\n", result); // Kết quả trận đấu

    time(&now);
    struct tm *end_tm = localtime(&now);
    fprintf(logfile, "Game ended: %s\n\n", asctime(end_tm)); // Ghi thời gian kết thúc

    fclose(logfile);
}

void send_log_to_players(int socket, const char *username, const char *opponent) {
    char buffer[BUF_SIZE];
    sprintf(buffer, "SEND_LOG %s %s", username, opponent);

    // Gửi thông tin để client nhận log
    send(socket, buffer, strlen(buffer), 0);
}

// Add this function to handle the move input and append to the move history:
void add_move_to_log(int moveX, int moveY, const char *player, int move_number) {
    // Append move to the log with a player and coordinates
    char move_entry[256];
    sprintf(move_entry, "Move %d: Player %s moved to (%d, %d)\n", move_number, player, moveX, moveY);
    strcat(moves, move_entry);
}

void printChessboard(void) {
    int moveX, moveY;
    #ifdef _WIN32
        system("cls"); // For Windows
    #else
        system("clear"); // For Linux/Unix/macOS
    #endif
    for (moveY = 0; moveY <= BOARD_SIZE; moveY++) {
        for (moveX = 0; moveX <= BOARD_SIZE; moveX++) {
            if (moveX == 0)
                printf("%3d", moveY);
            else if (moveY == 0)
                printf("%3d", moveX);
            else if (chessboard[moveX][moveY] == 1)
                printf("  X");
            else if (chessboard[moveX][moveY] == 2)
                printf("  O");
            else
                printf("  *");
        }
        printf("\n");
    }
}

// Process unexpected responses
void *process_server_response_unexpected(void *arg) {
    int socket = *(int *)arg;
    while (1) {
        char buffer[BUF_SIZE];
        memset(buffer, 0, BUF_SIZE);
        int bytes_read = read(socket, buffer, BUF_SIZE - 1);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            //printf("[Server Response] %s\n", buffer);

            // If the server sends a challenge request
            if (strstr(buffer, "\"type\": \"challenge\"") != NULL) {
                char *start = strstr(buffer, "\"from\": \"");
                if (start) {
                    start += strlen("\"from\": \"");
                    char *end = strchr(start, '\"');
                    if (end) {
                        *end = '\0';
                        strcpy(challenger, start);
                        printf("Do you accept the challenge from %s? (YES/NO):\n", challenger);
                        waiting_for_input = 1;
                    }
                }
            }
            // Handle game start
            else if (strstr(buffer, "\"type\": \"success\"") != NULL && strstr(buffer, "Game starts now!") != NULL) {

                char *start = strstr(buffer, "\"from\": \"");
                if (start) {
                    start += strlen("\"from\": \"");
                    char *end = strchr(start, '\"');
                    if (end) {
                        *end = '\0';
                        strcpy(opponent, start);
                    }
                }
                printf("The game has started!\n");
                printf("Enter command (MOVE):\n");
                game_active = 1;
                your_turn =1;
            }
            else if (strstr(buffer, "\"type\": \"error\"") != NULL) {
                printf("Error: %s\n", buffer);
                waiting_for_input = 0;
            }
            else if (strstr(buffer, "\"type\": \"lose_move\"") != NULL) {
               
                int moveX, moveY;
                sscanf(buffer, "{\"type\": \"lose_move\", \"move_x\": \"%d\",\"move_y\": \"%d\"}", &moveX, &moveY);
                chessboard[moveX][moveY] = 2;  // Äá»‘i thá»§ Ä‘Ă¡nh "O"
                printChessboard();  // Hiá»ƒn thá»‹ bĂ n cá» cáº­p nháº­t
                printf("Game over! %s wins!\n", opponent);
                //TODO: GET LOG
                // Ghi log kết thúc trận đấu
                char moves[1024] = "List of moves here"; // Lấy danh sách các nước đi từ lịch sử game

                log_game(player, opponent, "127.0.0.1", "127.0.0.1", moves, "win");

                // Gửi log cho người chơi
                send_log_to_players(socket, player, opponent);
                
                your_turn = 0;
                game_active = 0;
                printf("Enter command (LIST/LOGOUT/CHALLENGE):\n");
            }
            else if (strstr(buffer, "\"type\": \"opponent_move\"") != NULL) {
            int moveX, moveY;
            sscanf(buffer, "{\"type\": \"opponent_move\", \"move_x\": \"%d\",\"move_y\": \"%d\"}", &moveX, &moveY);

            if (moveX >= 1 && moveX <= BOARD_SIZE && moveY >= 1 && moveY <= BOARD_SIZE) {
                chessboard[moveX][moveY] = 2;  // Äá»‘i thá»§ Ä‘Ă¡nh "O"
                add_move_to_log(moveX, moveY, opponent, move_number);  // Log the opponent's move
                printChessboard();  // Hiá»ƒn thá»‹ bĂ n cá» cáº­p nháº­t
                printf("Opponent moved: %d %d\n", moveX, moveY);
                printf("Your turn.\n");
                printf("Enter command (MOVE):\n");
                move_number++;  // Increment move number for next move
                your_turn =1;
            } else {
                printf("Invalid move from opponent!\n");
            }
        }
        }
        memset(buffer, 0, BUF_SIZE); // Clear the buffer before reading new data
    }
    return NULL;
}

// Handle expected server responses
void process_server_response_expected(int socket, char *buffer, const char *username) {
    //printf("Processing server response (expected)...\n");
    memset(buffer, 0, BUF_SIZE);
    int bytes_read = read(socket, buffer, BUF_SIZE - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        //printf("[Server Response] %s\n", buffer);
        if (strstr(buffer, "\"type\": \"occupy_cell\"") != NULL)
        {
            printf("Invalid move! Try again.\n");
        }
        else if (strstr(buffer, "\"type\": \"process_move\"") != NULL)
        {
            your_turn = 0;
            printChessboard();  // Hiá»ƒn thá»‹ bĂ n cá»
        }
        else if (strstr(buffer, "\"type\": \"error\"") != NULL && strstr(buffer, "Rank difference too large") != NULL) {
            printf("Rank difference is too large to challenge.\n");
        }
        else if (strstr(buffer, "\"type\": \"win_move\"") != NULL) {
                
                int moveX, moveY;
                sscanf(buffer, "{\"type\": \"win_move\", \"move_x\": \"%d\",\"move_y\": \"%d\"}", &moveX, &moveY);
                chessboard[moveX][moveY] = 1;  // Äá»‘i thá»§ Ä‘Ă¡nh "O"
                add_move_to_log(moveX, moveY, player, move_number);  // Add the last move to log
                printChessboard();  // Hiá»ƒn thá»‹ bĂ n cá» cáº­p nháº­t
                printf("Game over! %s wins!\n", player);
                //TODO: GET LOG
                log_game(player, opponent, "127.0.0.1", "127.0.0.1", moves, player); 
                your_turn = 0;
                game_active = 0;
        }
        else if (strstr(buffer, "\"type\": \"draw_game\"") != NULL) {
            printChessboard();  
            printf("Game over! It's a draw.\n");
        }
    } else {
        printf("Error receiving server response.\n");
    }
}

// Command handling function
void handle_command(int socket, char *command, char *username, char *password, int *logged_in) {
    char buffer[BUF_SIZE];
    char target_user[50];

    char response_message[BUF_SIZE];
    

    if(game_active)
    {
        if (strncmp(command, "MOVE", 4) == 0 && your_turn) {
            int moveX = 0, moveY = 0;
            printf("Enter your move (x y): ");
            
            if (scanf("%d %d", &moveX, &moveY) == 2) {
                if (moveX >= 1 && moveX <= BOARD_SIZE && moveY >= 1 && moveY <= BOARD_SIZE && chessboard[moveX][moveY] == 0) 
                {
                    chessboard[moveX][moveY] = 1;  // NgÆ°á»i chÆ¡i Ä‘Ă¡nh "X"
                    add_move_to_log(moveX, moveY, player, move_number);  // Log the move.
                    sprintf(buffer, "MOVE %d %d %s", moveX, moveY, opponent);
                    
                    
                    send(socket, buffer, strlen(buffer), 0);
                    process_server_response_expected(socket, buffer, username);
                    move_number++;  // Increment move number

                    // Send the move to the server
                    if(game_active)
                        printf("Waiting for opponent's move...\n");
                } 
                else {
                    printf("Invalid move! Try again.\n");
                }
            } 
        }
    }
    else
    {
        if(waiting_for_input == 1)
            handle_exception_command(command, response_message, socket);
        else
            handle_reguler_comand(command, logged_in, username, password, buffer, socket, target_user);
    }
}

void handle_exception_command(char *command, char response_message[1024], int socket)
{
    if (strcasecmp(command, "YES") == 0)
    {
        game_active = 1;
        strcpy(opponent,challenger);
        sprintf(response_message, "RESPONSE_CHALLENGE YES %s", challenger);
        send(socket, response_message, strlen(response_message), 0);
        printf("You accepted the challenge from %s.\n", challenger);
    }
    else if (strcasecmp(command, "NO") == 0)
    {
        sprintf(response_message, "RESPONSE_CHALLENGE NO %s", challenger);
        send(socket, response_message, strlen(response_message), 0);
        printf("You declined the challenge from %s.\n", challenger);
    }
}

void handle_reguler_comand(char *command, int *logged_in, char *username, char *password, char buffer[1024], int socket, char target_user[50])
{
    if (strcmp(command, "REGISTER") == 0 && !*logged_in)
    {
        printf("Enter username: ");
        scanf("%s", username);
        printf("Enter password: ");
        scanf("%s", password);

        sprintf(buffer, "REGISTER %s %s", username, password);
        send(socket, buffer, strlen(buffer), 0);
        process_server_response_expected(socket, buffer, username);
    }
    else if (strcmp(command, "LOGIN") == 0 && !*logged_in) {
        printf("Enter username: ");
        scanf("%49s", username);  // Limit input to 49 characters
        printf("Enter password: ");
        scanf("%49s", password);  // Limit input to 49 characters

        sprintf(buffer, "LOGIN %s %s", username, password);
        send(socket, buffer, strlen(buffer), 0);
        
        // Clear the buffer before processing the server response
        memset(buffer, 0, BUF_SIZE);
        process_server_response_expected(socket, buffer, username);
        
        if (strstr(buffer, "SUCCESS") != NULL) {
            *logged_in = 1;
            strcpy(player, username);
            printf("Login successful.\n");
        } else {
            printf("Login failed.\n");
        }
    }
    else if (strcmp(command, "LIST") == 0 && *logged_in)
    {
        sprintf(buffer, "LIST %s", username);
        send(socket, buffer, strlen(buffer), 0);
        process_server_response_expected(socket, buffer, username);

         // Parse the response to display users in the client
        if (strstr(buffer, "\"type\": \"user_list\"") != NULL) {
            printf("Online Users:\n");

            char *data_start = strstr(buffer, "\"data\": [");
            if (data_start) {
                data_start += strlen("\"data\": [");
                while (1) {
                    char username[50];
                    int rank;
                    int n = sscanf(data_start, "{\"username\": \"%49[^\"]\", \"rank\": %d}", username, &rank);

                    if (n == 2) {
                        printf("- %s (Rank: %d)\n", username, rank);
                    } else {
                        break;
                    }

                    // Move to the next item in the JSON array
                    data_start = strstr(data_start, "},");
                    if (!data_start) break;
                    data_start += 2;  // Move past "},"
                }
            }
        } else {
            printf("Error fetching user list: %s\n", buffer);
        }
    }
    else if (strcmp(command, "LOGOUT") == 0 && *logged_in)
    {
        sprintf(buffer, "LOGOUT %s", username);
        send(socket, buffer, strlen(buffer), 0);
        process_server_response_expected(socket, buffer, username);
        *logged_in = 0;
    }
    else if (strcmp(command, "CHALLENGE") == 0 && *logged_in)
    {
        printf("Enter username to challenge: ");
        scanf("%s", target_user);

        sprintf(buffer, "CHALLENGE %s %s", username, target_user);
        send(socket, buffer, strlen(buffer), 0);
        process_server_response_expected(socket, buffer, username);
    }
    else
    {
        printf("Invalid command or login required.\n");
    }
}

// Main server communication loop
void communicate_with_server(int socket) {
    char command[20], username[50], password[50];
    int logged_in = 0;

    while (1) {
        if (logged_in) {
             if(game_active)
                {
                    if(your_turn)
                        printf("Enter command (MOVE):\n");
                    else
                        printf("Waiting for opponent.....\n");
                }
            else
                {
                    printf("Enter command (LIST/LOGOUT/CHALLENGE):\n");
                }
                 
        } 
        else 
        {
            printf("Enter command (REGISTER/LOGIN):\n");
        }
        scanf("%s", command);
        handle_command(socket, command, username, password, &logged_in);
    }
}

int main() {
    int sock = 0;
    struct sockaddr_in server_address;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error\n");
        return -1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
        perror("Invalid address\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed\n");
        return -1;
    }

    printf("Connected to server.\n");
    pthread_t response_thread;
    pthread_create(&response_thread, NULL, process_server_response_unexpected, &sock);
    communicate_with_server(sock);

    close(sock);
    return 0;
}
