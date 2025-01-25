#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define PORT 12345
#define BUFFER_SIZE 1024
#define MAX_USERNAME 50
#define MAX_PASSWORD 50

void clear_screen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

void show_menu() {
    printf("\n=== Number Guessing Game ===\n");
    printf("1. Login\n");
    printf("2. Register\n");
    printf("3. Exit\n");
    printf("Choice: ");
}

void play_game(int client_socket) {
    char buffer[BUFFER_SIZE];
    int guess;
    
    clear_screen();
    printf("\n=== Game Started ===\n");
    printf("Guess a number between 1 and 100\n");
    printf("Type 0 to logout\n\n");
    
    while (1) {
        printf("Enter your guess: ");
        if (scanf("%d", &guess) != 1) {
            printf("Invalid input. Please enter a number.\n");
            while (getchar() != '\n'); // Clear input buffer
            continue;
        }
        
        if (guess == 0) {
            strcpy(buffer, "LOGOUT");
            send(client_socket, buffer, strlen(buffer), 0);
            break;
        }
        
        if (guess < 1 || guess > 100) {
            printf("Please enter a number between 1 and 100.\n");
            continue;
        }
        
        sprintf(buffer, "%d", guess);
        send(client_socket, buffer, strlen(buffer), 0);
        
        memset(buffer, 0, sizeof(buffer));
        recv(client_socket, buffer, sizeof(buffer), 0);
        
        char *command = strtok(buffer, "|");
        
        if (strcmp(command, "LOWER") == 0) {
            printf("Too high! Try a lower number (Tries: %s)\n", strtok(NULL, "|"));
        }
        else if (strcmp(command, "HIGHER") == 0) {
            printf("Too low! Try a higher number (Tries: %s)\n", strtok(NULL, "|"));
        }
        else if (strcmp(command, "WIN") == 0) {
            int tries = atoi(strtok(NULL, "|"));
            int high_score = atoi(strtok(NULL, "|"));
            int games_played = atoi(strtok(NULL, "|"));
            
            printf("\nCongratulations! You won in %d tries!\n", tries);
            printf("Your best score: %d\n", high_score);
            printf("Total games played: %d\n\n", games_played);
            printf("New game starting...\n\n");
        }
    }
}

int main() {
    int client_socket;
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE];
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    int connected = 0;
    
    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Setup server address
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_address.sin_port = htons(PORT);
    
    // Connect to server
    if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
    
    while (1) {
        if (!connected) {
            clear_screen();
            show_menu();
            
            int choice;
            if (scanf("%d", &choice) != 1) {
                printf("Invalid input. Please enter a number.\n");
                while (getchar() != '\n'); // Clear input buffer
                sleep(2);
                continue;
            }
            getchar(); // Clear newline
            
            if (choice == 3) {
                break;
            }
            
            if (choice != 1 && choice != 2) {
                printf("Invalid choice. Please select 1, 2, or 3.\n");
                sleep(2);
                continue;
            }
            
            printf("Username: ");
            fgets(username, MAX_USERNAME, stdin);
            username[strcspn(username, "\n")] = 0;
            
            printf("Password: ");
            fgets(password, MAX_PASSWORD, stdin);
            password[strcspn(password, "\n")] = 0;
            
            sprintf(buffer, "%s %s %s", 
                choice == 1 ? "LOGIN" : "REGISTER",
                username,
                password);
            
            send(client_socket, buffer, strlen(buffer), 0);
            
            memset(buffer, 0, sizeof(buffer));
            recv(client_socket, buffer, sizeof(buffer), 0);
            
            int result = atoi(buffer);
            
            if (result == 1) {
                printf("\nSuccess! Starting game...\n");
                sleep(1);
                connected = 1;
                play_game(client_socket);
                connected = 0;
            }
            else if (result == -2) {
                printf("\nError: User already logged in\n");
                printf("Press Enter to continue...");
                getchar();
            }
            else if (result == 0) {
                printf("\nError: Invalid credentials or username taken\n");
                printf("Press Enter to continue...");
                getchar();
            }
            else {
                printf("\nError: Invalid command\n");
                printf("Press Enter to continue...");
                getchar();
            }
        }
    }
    
    close(client_socket);
    return 0;
}