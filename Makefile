# Biến để dễ quản lý
CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lpthread
TARGETS = server client

# Các file nguồn và file đối tượng
SERVER_SRC = server.c game.c
SERVER_OBJ = $(SERVER_SRC:.c=.o)

CLIENT_SRC = client.c
CLIENT_OBJ = $(CLIENT_SRC:.c=.o)

# Đích mặc định
all: $(TARGETS)

# Build server
server: $(SERVER_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build client
client: $(CLIENT_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Quy tắc chung để tạo file .o từ file .c
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(TARGETS) *.o
