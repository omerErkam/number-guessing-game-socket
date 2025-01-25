#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <stdarg.h>  // Added for va_start and va_end

#define PORT 12345
#define BUFFER_SIZE 1024
#define MAX_USERS 100
#define MAX_USERNAME 50
#define MAX_PASSWORD 50

// Rest of the code remains exactly the same as in the previous artifact...
// Structure to hold user data
typedef struct {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    int high_score;
    int games_played;
    int current_number;
    int current_tries;
    int is_logged_in;
} User;

// Global variables
User users[MAX_USERS];
int num_users = 0;
pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function to get current timestamp
char* get_timestamp() {
    static char buffer[26];
    time_t timer;
    struct tm* tm_info;

    time(&timer);
    tm_info = localtime(&timer);
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

// Function to print server message with timestamp
void server_log(const char* format, ...) {
    pthread_mutex_lock(&print_mutex);
    va_list args;
    va_start(args, format);
    printf("[%s] ", get_timestamp());
    vprintf(format, args);
    printf("\n");
    fflush(stdout);
    va_end(args);
    pthread_mutex_unlock(&print_mutex);
}

// Function to save users to file
void save_users() {
    FILE *file = fopen("users.dat", "w");
    if (file != NULL) {
        for (int i = 0; i < num_users; i++) {
            fprintf(file, "%s %s %d %d\n", 
                users[i].username, 
                users[i].password, 
                users[i].high_score, 
                users[i].games_played);
        }
        fclose(file);
        server_log("User data saved to users.dat");
    }
}

// Function to load users from file
void load_users() {
    FILE *file = fopen("users.dat", "r");
    if (file != NULL) {
        while (num_users < MAX_USERS && 
               fscanf(file, "%s %s %d %d", 
                   users[num_users].username, 
                   users[num_users].password, 
                   &users[num_users].high_score, 
                   &users[num_users].games_played) == 4) {
            users[num_users].is_logged_in = 0;
            users[num_users].current_number = rand() % 100 + 1;
            users[num_users].current_tries = 0;
            num_users++;
        }
        fclose(file);
        server_log("Loaded %d users from users.dat", num_users);
    }
}

// Function to find user by username
int find_user(const char *username) {
    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].username, username) == 0) {
            return i;
        }
    }
    return -1;
}

// Function to handle login/register
int handle_auth(char *buffer, int *user_index) {
    char command[10], username[MAX_USERNAME], password[MAX_PASSWORD];
    sscanf(buffer, "%s %s %s", command, username, password);
    
    pthread_mutex_lock(&users_mutex);
    
    if (strcmp(command, "LOGIN") == 0) {
        *user_index = find_user(username);
        if (*user_index != -1 && strcmp(users[*user_index].password, password) == 0) {
            if (!users[*user_index].is_logged_in) {
                users[*user_index].is_logged_in = 1;
                server_log("User '%s' logged in successfully", username);
                pthread_mutex_unlock(&users_mutex);
                return 1; // Success
            }
            server_log("Login failed for user '%s': Already logged in", username);
            pthread_mutex_unlock(&users_mutex);
            return -2; // Already logged in
        }
        server_log("Login failed for user '%s': Invalid credentials", username);
        pthread_mutex_unlock(&users_mutex);
        return 0; // Invalid credentials
    }
    else if (strcmp(command, "REGISTER") == 0) {
        if (find_user(username) == -1 && num_users < MAX_USERS) {
            strcpy(users[num_users].username, username);
            strcpy(users[num_users].password, password);
            users[num_users].high_score = 0;
            users[num_users].games_played = 0;
            users[num_users].current_number = rand() % 100 + 1;
            users[num_users].current_tries = 0;
            users[num_users].is_logged_in = 1;
            *user_index = num_users;
            num_users++;
            save_users();
            server_log("New user registered: '%s'", username);
            pthread_mutex_unlock(&users_mutex);
            return 1; // Success
        }
        server_log("Registration failed for user '%s': Username taken or server full", username);
        pthread_mutex_unlock(&users_mutex);
        return 0; // Username taken or server full
    }
    
    pthread_mutex_unlock(&users_mutex);
    return -1; // Invalid command
}

// Function to print game statistics
void print_game_stats() {
    pthread_mutex_lock(&users_mutex);
    int active_users = 0;
    int total_games = 0;
    
    for (int i = 0; i < num_users; i++) {
        if (users[i].is_logged_in) {
            active_users++;
        }
        total_games += users[i].games_played;
    }
    
    server_log("=== Server Statistics ===");
    server_log("Total users: %d", num_users);
    server_log("Active users: %d", active_users);
    server_log("Total games played: %d", total_games);
    pthread_mutex_unlock(&users_mutex);
}

// Function to handle game logic
void handle_game(int client_socket, int user_index) {
    char buffer[BUFFER_SIZE];
    int guess;
    
    server_log("Starting new game for user '%s'", users[user_index].username);
    
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        if (recv(client_socket, buffer, sizeof(buffer), 0) <= 0) {
            break;
        }
        
        if (strcmp(buffer, "LOGOUT") == 0) {
            pthread_mutex_lock(&users_mutex);
            users[user_index].is_logged_in = 0;
            save_users();
            server_log("User '%s' logged out", users[user_index].username);
            print_game_stats();
            pthread_mutex_unlock(&users_mutex);
            break;
        }
        
        guess = atoi(buffer);
        pthread_mutex_lock(&users_mutex);
        users[user_index].current_tries++;
        
        char response[BUFFER_SIZE];
        if (guess > users[user_index].current_number) {
            sprintf(response, "LOWER|%d", users[user_index].current_tries);
            server_log("User '%s' guessed %d (too high) - Attempt %d", 
                users[user_index].username, guess, users[user_index].current_tries);
        }
        else if (guess < users[user_index].current_number) {
            sprintf(response, "HIGHER|%d", users[user_index].current_tries);
            server_log("User '%s' guessed %d (too low) - Attempt %d", 
                users[user_index].username, guess, users[user_index].current_tries);
        }
        else {
            if (users[user_index].high_score == 0 || 
                users[user_index].current_tries < users[user_index].high_score) {
                users[user_index].high_score = users[user_index].current_tries;
            }
            users[user_index].games_played++;
            sprintf(response, "WIN|%d|%d|%d", 
                users[user_index].current_tries,
                users[user_index].high_score,
                users[user_index].games_played);
                
            server_log("User '%s' won in %d tries! (Best: %d, Total games: %d)", 
                users[user_index].username,
                users[user_index].current_tries,
                users[user_index].high_score,
                users[user_index].games_played);
                
            users[user_index].current_number = rand() % 100 + 1;
            users[user_index].current_tries = 0;
            save_users();
            print_game_stats();
        }
        pthread_mutex_unlock(&users_mutex);
        
        send(client_socket, response, strlen(response), 0);
    }
}

// Thread function to handle each client
void *handle_client(void *arg) {
    int client_socket = *(int*)arg;
    free(arg);
    
    char buffer[BUFFER_SIZE];
    int user_index;
    
    // Handle authentication
    memset(buffer, 0, sizeof(buffer));
    if (recv(client_socket, buffer, sizeof(buffer), 0) <= 0) {
        close(client_socket);
        return NULL;
    }
    
    int auth_result = handle_auth(buffer, &user_index);
    sprintf(buffer, "%d", auth_result);
    send(client_socket, buffer, strlen(buffer), 0);
    
    if (auth_result == 1) {
        handle_game(client_socket, user_index);
    }
    
    close(client_socket);
    return NULL;
}

int main() {
    int server_socket, *client_socket;
    struct sockaddr_in server_address, client_address;
    pthread_t thread_id;
    
    // Initialize random number generator
    srand(time(NULL));
    
    // Load existing users
    load_users();
    
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Setup server address
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);
    
    // Bind socket
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if (listen(server_socket, 5) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    server_log("=== Number Guessing Game Server ===");
    server_log("Server started on port %d", PORT);
    print_game_stats();
    
    while (1) {
        socklen_t client_len = sizeof(client_address);
        client_socket = malloc(sizeof(int));
        *client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_len);
        
        if (*client_socket < 0) {
            free(client_socket);
            continue;
        }
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_address.sin_addr), client_ip, INET_ADDRSTRLEN);
        server_log("New connection from %s", client_ip);
        
        pthread_create(&thread_id, NULL, handle_client, (void*)client_socket);
        pthread_detach(thread_id);
    }
    
    close(server_socket);
    return 0;
}