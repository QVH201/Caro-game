#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "game.h"

#define PORT 8080
#define BUF_SIZE 1024
#define MAX_USERS 50
#define BOARD_SIZE 8


typedef struct {
    char username[50];
    char password[50];
    int logged_in;
    int socket;
    int rank;
} User;

typedef struct {
    char player1[50];
    char player2[50];
    char ip1[50]; // Địa chỉ IP của player1
    char ip2[50]; // Địa chỉ IP của player2
    char moves[1024]; // Danh sách các nước đi
    char result[50];  // Kết quả trận đấu (thắng, thua, hòa)
    time_t start_time; // Thời gian bắt đầu
    time_t end_time;   // Thời gian kết thúc
} game_log;

// Global variables
User users[MAX_USERS];
int user_count = 0;

char player1[50] = "", player2[50] = "";
int game_active = 0;

void save_users_to_file() {
    FILE *file = fopen("data.txt", "w");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    for (int i = 0; i < user_count; i++) {
        fprintf(file, "%s %s %d\n", users[i].username, users[i].password, users[i].rank);
    }

    fclose(file);
}

void load_users_from_file() {
    FILE *file = fopen("data.txt", "r");
    if (file == NULL) {
        return; 
    }

    while (fscanf(file, "%s %s %d", users[user_count].username, users[user_count].password, &users[user_count].rank) != EOF) {
        users[user_count].logged_in = 0;
        users[user_count].socket = -1;  
        user_count++;
    }

    fclose(file);
}



void send_log_file(int socket) {
    FILE *file = fopen("game_log.txt", "rb");
    if (!file) {
        printf("Error opening log file for sending!\n");
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    // Gửi kích thước file trước
    send(socket, &file_size, sizeof(file_size), 0);

    // Gửi nội dung file
    char buffer[BUF_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(socket, buffer, bytes_read, 0);
    }

    fclose(file);
}

// User authentication functions
int register_user(const char *username, const char *password) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0)
            return 0; // Username already exists
    }

    strcpy(users[user_count].username, username);
    strcpy(users[user_count].password, password);
    users[user_count].rank = 1;  // Default rank is 1
    users[user_count].logged_in = 0;
    users[user_count].socket = -1;  // Socket will be set after login
    user_count++;

    save_users_to_file();  // Save updated list to file

    return 1; // Registration successful
}

void add_online_user(const char *username) {

    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            return; 
        }
    }

    if (user_count < MAX_USERS) {
        strcpy(users[user_count].username, username);
        users[user_count].logged_in = 1; 
        user_count++;
    }
}

void remove_online_user(const char *username) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            users[i].logged_in = 0;  // Mark as logged out
            // Shift remaining users down
            for (int j = i; j < user_count - 1; j++) {
                users[j] = users[j + 1];
            }
            user_count--;
            break;
        }
    }
}

char *get_online_users(int userRank) {
    //TODO: chi lay user tren duoi 10 rank
    
    char *response = malloc(BUF_SIZE);
    if (response == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    strcpy(response, "{\"type\": \"user_list\", \"data\": [");

    int first = 1; // Cá» Ä‘á»ƒ kiá»ƒm tra pháº§n tá»­ Ä‘áº§u tiĂªn
    for (int i = 0; i < user_count; i++) {
        if (users[i].logged_in && abs(users[i].rank - userRank) <= 10) {
            char user_info[100]; 
            snprintf(user_info, sizeof(user_info), "{\"username\": \"%s\", \"rank\": %d}", users[i].username, users[i].rank);
            if (!first) { // Náº¿u khĂ´ng pháº£i pháº§n tá»­ Ä‘áº§u tiĂªn, thĂªm dáº¥u pháº©y trÆ°á»›c Ä‘Ă³
                strcat(response, ",");
            }
            strcat(response, user_info);
            first = 0;
        }
    }

    strcat(response, "]}");
    return response;
}

int login_user(const char *username, const char *password, int client_socket) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0 && strcmp(users[i].password, password) == 0) {
            if (users[i].logged_in) {
                return -1; 
            }
            users[i].logged_in = 1;  
            users[i].socket = client_socket;

            add_online_user(username); 
            return 1;  
        }
    }
    return 0; 
}

void logout_user(const char *username) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            users[i].logged_in = 0;
            users[i].socket = -1;  // Clear the socket after logout
        }
    }
}

// Game logic functions
void reset_game() {
    memset(chessboard, 0, sizeof(chessboard));
    whoseTurn = 1; 
    game_active = 0;
}

// Command handler
void handle_command(int client_socket, const char *command) {
    char response[BUF_SIZE];
    char username[50], password[50];

    if (strncmp(command, "REGISTER", 8) == 0) {
        sscanf(command, "REGISTER %s %s", username, password);
        if (register_user(username, password)) {
            sprintf(response, "SUCCESS: User %s registered.\n", username);
        } else {
            sprintf(response, "FAIL: Username %s already exists.\n", username);
        }
        send(client_socket, response, strlen(response), 0);
    } else if (strncmp(command, "LOGIN", 5) == 0) {
        sscanf(command, "LOGIN %s %s", username, password);
        int login_status = login_user(username, password, client_socket);
        if (login_status == 1) {
            sprintf(response, "SUCCESS: User %s logged in.\n", username);
            add_online_user(username);  // Add user to online list
        } else if (login_status == -1) {
            sprintf(response, "FAIL: User %s already logged in.\n", username);
        } else {
            sprintf(response, "FAIL: Incorrect username or password.\n");
        }
        printf("Sent: %s\n", response);
        send(client_socket, response, strlen(response), 0);
    }
    else if (strncmp(command, "LIST", 4) == 0) {
        sscanf(command, "LIST %s", username);
        int playerRank = -1;
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].username, username) == 0 && users[i].logged_in) {
                playerRank = users[i].rank;
                break;
            }
        }
        char *response = get_online_users(playerRank);
        send(client_socket, response, strlen(response), 0);
        free(response);
    } else if (strncmp(command, "LOGOUT", 6) == 0) {
        sscanf(command, "LOGOUT %s", username);
        logout_user(username);
        sprintf(response, "SUCCESS: User %s logged out.\n", username);
        send(client_socket, response, strlen(response), 0);
    } else if (strncmp(command, "CHALLENGE", 9) == 0) {
        char player1[50], player2[50];
        sscanf(command, "CHALLENGE %s %s", player1, player2);

        int player1_rank = -1, player2_rank = -1;
        int player2_socket = -1;
        // Lấy rank của player1
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].username, player1) == 0 && users[i].logged_in) {
                player1_rank = users[i].rank;
                break;
            }
        }
        // Lấy rank của player2 và kiểm tra xem player2 có đang online không
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].username, player2) == 0 && users[i].logged_in) {
                player2_rank = users[i].rank;
                player2_socket = users[i].socket;
                break;
            }
        }

        if (player2_socket == -1) {
        
            sprintf(response, "{\"type\": \"error\", \"message\": \"Player %s not online.\"}", player2);
            send(client_socket, response, strlen(response), 0);
        }else if (abs(player1_rank - player2_rank) > 10) {  // Kiểm tra rank chênh lệch
            sprintf(response, "{\"type\": \"error\", \"message\": \"Rank difference too large. Challenge cannot proceed.\"}");
            send(client_socket, response, strlen(response), 0);
        }  
        else {
           
            sprintf(response, "{\"type\": \"challenge\", \"from\": \"%s\"}", player1);
            send(player2_socket, response, strlen(response), 0);
            printf("Sent: %s\n", response);
            sprintf(response, "Your challenge has been sent to %s.\n", player2);
            send(client_socket, response, strlen(response), 0);
        }
    }
    else if (strncmp(command, "RESPONSE_CHALLENGE", 18) == 0) {
        char choice[4];
        char challenger[50];
        sscanf(command, "RESPONSE_CHALLENGE %s %s", choice,challenger );

        int player2_socket = -1;
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].username, challenger) == 0 && users[i].logged_in) {
                player2_socket = users[i].socket;
                break;
            }
        }
        if (strcmp(choice, "YES") == 0) {
            sprintf(response, "{\"type\": \"success\", \"from\": \"%s\", \"message\": \"Challenge accepted! Game starts now!\" }",username);
            send(player2_socket, response, strlen(response), 0);
            printf("Sent: %s\n", response);
         
        } else {
            sprintf(response, "{\"type\": \"error\", \"message\": \"Challenge declined.\"}");
            send(player2_socket, response, strlen(response), 0);
            printf("Sent: %s\n", response);
        }
    }
    else if (strncmp(command, "MOVE", 4) == 0)
    {
        int x, y;
        char opponent[50];
        
        sscanf(command, "MOVE %d %d %s", &x, &y, opponent);

        char response[BUF_SIZE]; // Sá»­ dá»¥ng response Ä‘á»ƒ lÆ°u trá»¯ thĂ´ng Ä‘iá»‡p
        
        int player2_socket = -1;
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].username, opponent) == 0 && users[i].logged_in) {
                player2_socket = users[i].socket;
                break;
            }
        }

        // Kiá»ƒm tra xem vá»‹ trĂ­ Ä‘Ă£ Ä‘Æ°á»£c chiáº¿m hay chÆ°a
        if (chessboard[x][y] != 0) { 
            snprintf(response, sizeof(response), "{\"type\": \"occupy_cell\", \"message\": \"Invalid move, cell already occupied.\"}");
            send(client_socket, response, strlen(response), 0);
            printf("Sent: %s\n", response);  // Log ra thĂ´ng Ä‘iá»‡p gá»­i Ä‘i
            return;
        }

        // Thá»±c hiá»‡n nÆ°á»›c cá»
        chessboard[x][y] = whoseTurn;

        int winner = judge(whoseTurn, x, y);  // Kiá»ƒm tra tháº¯ng thua
        if (winner) {
            //TODO
            //CHANGE TWO PLAYER RANK AND SAVE TO FILE.
            for (int i = 0; i < user_count; i++) {
                if (strcmp(users[i].username, username) == 0) {
                    users[i].rank += 1; // Người thắng
                }
                if (strcmp(users[i].username, opponent) == 0) {
                    users[i].rank -= 1; // Người thua
                    if (users[i].rank < 1) users[i].rank = 1; // Đảm bảo rank không nhỏ hơn 1
                }
            }

            // Save updated ranks to file
            save_users_to_file();

            snprintf(response, sizeof(response), "{\"type\": \"win_move\", \"move_x\": \"%d\",\"move_y\": \"%d\"}",x,y);
            send(client_socket, response, strlen(response), 0); // Gá»­i káº¿t quáº£ tháº¯ng cho client
            printf("Sent: %s\n", response);  // Log ra thĂ´ng Ä‘iá»‡p gá»­i Ä‘i
            snprintf(response, sizeof(response), "{\"type\": \"lose_move\", \"move_x\": \"%d\",\"move_y\": \"%d\"}",x,y);
            send(player2_socket, response, strlen(response), 0); // Gá»­i káº¿t quáº£ tháº¯ng cho client

            //TODO 
            //SEND LOG TO PLAYER

            game_active = 0; // Káº¿t thĂºc trĂ² chÆ¡i
        } else {
            // Äá»•i lÆ°á»£t
            whoseTurn = (whoseTurn == 1) ? 2 : 1;

            // Gá»­i thĂ´ng bĂ¡o Ä‘á»ƒ yĂªu cáº§u Ä‘á»‘i thá»§ tiáº¿p tá»¥c
            snprintf(response, sizeof(response), "{\"type\": \"process_move\", \"data\": {}}");
            send(client_socket, response, strlen(response), 0);
            printf("Sent: %s\n", response);  // Log ra thĂ´ng Ä‘iá»‡p gá»­i Ä‘i
            snprintf(response, sizeof(response), "{\"type\": \"opponent_move\", \"move_x\": \"%d\",\"move_y\": \"%d\"}",x,y);
            send(player2_socket, response, strlen(response), 0);
        }

        printf("Sent: %s\n", response);  // Log ra thĂ´ng Ä‘iá»‡p gá»­i Ä‘i
    }
}

void *client_thread(void *arg) {
    int client_socket = *(int *)arg;
    char buffer[BUF_SIZE];

    printf("Client connected.\n");
    
    while (1) {
        memset(buffer, 0, BUF_SIZE);
        int bytes_received = recv(client_socket, buffer, BUF_SIZE, 0);
        if (bytes_received <= 0) {
            printf("Client disconnected.\n");
            close(client_socket);
            pthread_exit(NULL);
        }

        printf("Received: %s\n", buffer);
        handle_command(client_socket, buffer);
    }
    return NULL;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_len = sizeof(client_address);

    reset_game(); // Initialize the game board
    load_users_from_file();  // Load users from file

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        return -1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Bind failed");
        close(server_socket);
        return -1;
    }

    if (listen(server_socket, 10) == -1) {
        perror("Listen failed");
        close(server_socket);
        return -1;
    }

    printf("Server is running on port %d...\n", PORT);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_len);
        if (client_socket == -1) {
            perror("Client connection failed");
            continue;
        }

        
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, client_thread, &client_socket);
        pthread_detach(thread_id);
    }

    close(server_socket);
    return 0;
}
