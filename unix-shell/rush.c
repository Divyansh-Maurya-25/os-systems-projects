/*
 * Name:    Divyansh Maurya
 * NetID:   U11865935
 * Description:
 *   This is my implementation of rush (Rapid Unix SHell) for the OS project.
 *   It's basically a simple shell that can run commands like bash does.
 *   
 *   Features I implemented:
 *   - Shows "rush> " prompt and waits for user input
 *   - Built-in commands: exit, cd, and path
 *   - Can run programs by searching through the PATH directories
 *   - Output redirection with > symbol (like ls > file.txt)
 *   - Handles spaces and tabs properly
 *   
 *   I'm using fork() and execv() to run external commands, and the shell
 *   keeps looping until user types "exit" or presses Ctrl+D.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>

#define MAX_ARGS 64
#define ERROR_MESSAGE "An error has occurred\n"


// Function to print error message to stderr
// The assignment requires us to use write() instead of printf for errors
void print_error() {
    write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
    fflush(stderr);
}


// This function removes whitespace from beginning and end of a string
// I use it to clean up the input line before processing
char *trim_whitespace(char *str) {
    if (str == NULL) {
        return NULL;
    }
    
    // Remove spaces/tabs from the beginning
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }
    
    // If string is empty after trimming start, just return
    if (*str == '\0') {
        return str;
    }
    
    // Now remove spaces/tabs from the end
    char *end_ptr = str + strlen(str) - 1;
    while (end_ptr > str && (*end_ptr == ' ' || *end_ptr == '\t' || 
           *end_ptr == '\n' || *end_ptr == '\r')) {
        *end_ptr = '\0';
        end_ptr--;
    }
    
    return str;
}


// Parse the input line into separate tokens (words)
// Returns array of strings, with NULL at the end
// Also returns count of tokens if needed
char **parse_input(char *input, int *token_count) {
    int capacity = 8;  // start with space for 8 tokens
    int count = 0;
    
    // Allocate array to hold pointers to tokens
    char **token_array = malloc(capacity * sizeof(char*));
    if (token_array == NULL) {
        print_error();
        exit(1);
    }
    
    // Loop through the input string
    while (1) {
        // Skip any whitespace
        while (*input == ' ' || *input == '\t') {
            input++;
        }
        
        // If we hit end of string, we're done
        if (*input == '\0') {
            break;
        }
        
        // Mark where this token starts
        char *token_start = input;
        
        // Find where this token ends (next space or end of string)
        while (*input != '\0' && *input != ' ' && *input != '\t') {
            input++;
        }
        
        // Calculate length of this token
        int token_length = input - token_start;
        
        // Allocate memory for this token and copy it
        char *new_token = malloc(token_length + 1);
        if (new_token == NULL) {
            print_error();
            exit(1);
        }
        strncpy(new_token, token_start, token_length);
        new_token[token_length] = '\0';
        
        // If array is full, make it bigger
        if (count >= capacity) {
            capacity = capacity * 2;
            token_array = realloc(token_array, capacity * sizeof(char*));
            if (token_array == NULL) {
                print_error();
                exit(1);
            }
        }
        
        // Add token to array
        token_array[count] = new_token;
        count++;
        
        if (*input == '\0') {
            break;
        }
    }
    
    // Make sure array has space for NULL terminator
    if (count >= capacity) {
        token_array = realloc(token_array, (capacity + 1) * sizeof(char*));
        if (token_array == NULL) {
            print_error();
            exit(1);
        }
    }
    
    // NULL terminate the array (required for execv)
    token_array[count] = NULL;
    
    if (token_count != NULL) {
        *token_count = count;
    }
    
    return token_array;
}


// Free memory allocated by parse_input
void free_token_array(char **tokens) {
    if (tokens == NULL) {
        return;
    }
    
    // Free each individual token
    for (int i = 0; tokens[i] != NULL; i++) {
        free(tokens[i]);
    }
    
    // Free the array itself
    free(tokens);
}


// Combine directory path and command name to make full path
// For example: "/bin" + "ls" = "/bin/ls"
char *make_full_path(const char *directory, const char *command) {
    int dir_len = strlen(directory);
    int cmd_len = strlen(command);
    
    // Allocate space for dir + "/" + cmd + null terminator
    char *full_path = malloc(dir_len + 1 + cmd_len + 1);
    if (full_path == NULL) {
        print_error();
        exit(1);
    }
    
    // Copy directory first
    strcpy(full_path, directory);
    
    // Add slash if directory doesn't end with one
    if (dir_len > 0 && directory[dir_len - 1] != '/') {
        strcat(full_path, "/");
    }
    
    // Add command name
    strcat(full_path, command);
    
    return full_path;
}


int main(int argc, char *argv[]) {
    
    // Check if shell was started with any arguments (not allowed)
    if (argc != 1) {
        print_error();
        exit(1);
    }
    
    // Initialize the PATH with just /bin directory
    char **search_path = malloc(sizeof(char*));
    if (search_path == NULL) {
        print_error();
        exit(1);
    }
    search_path[0] = strdup("/bin");
    int num_paths = 1;
    
    // Variables for reading input with getline
    char *input_line = NULL;
    size_t line_capacity = 0;
    ssize_t line_length;
    
    
    // Main loop - keep reading and executing commands
    while (1) {
        
        // Print the prompt
        printf("rush> ");
        fflush(stdout);  // make sure prompt appears immediately
        
        // Read a line of input from user
        line_length = getline(&input_line, &line_capacity, stdin);
        
        // Check if we got EOF (user pressed Ctrl+D)
        if (line_length == -1) {
            break;  // exit the shell
        }
        
        // Skip empty lines
        if (line_length == 0) {
            continue;
        }
        
        // Remove the newline character at end if it exists
        if (line_length > 0 && input_line[line_length - 1] == '\n') {
            input_line[line_length - 1] = '\0';
        }
        
        // Trim whitespace from beginning and end
        char *clean_input = trim_whitespace(input_line);
        
        // If line is empty after trimming, just continue (not an error)
        if (clean_input == NULL || *clean_input == '\0') {
            continue;
        }
        
        
        // Check for redirection symbol '>'
        // Count how many '>' symbols there are
        int redirect_count = 0;
        for (char *ptr = clean_input; *ptr != '\0'; ptr++) {
            if (*ptr == '>') {
                redirect_count++;
            }
        }
        
        // More than one '>' is an error
        if (redirect_count > 1) {
            print_error();
            continue;
        }
        
        // Variables for handling redirection
        char *command_part = NULL;
        char *filename_part = NULL;
        char *output_file = NULL;
        int redirect_output = 0;
        
        // If there's a redirection, split the command
        if (redirect_count == 1) {
            // Find the '>' character
            char *redirect_pos = strchr(clean_input, '>');
            
            // Split string at '>'
            *redirect_pos = '\0';
            command_part = trim_whitespace(clean_input);
            filename_part = trim_whitespace(redirect_pos + 1);
            
            // Check that we have a command before '>'
            if (command_part == NULL || *command_part == '\0') {
                print_error();
                continue;
            }
            
            // Check that we have a filename
            if (filename_part == NULL || *filename_part == '\0') {
                print_error();
                continue;
            }
            
            // The filename part should be just one word
            int file_count = 0;
            char **file_tokens = parse_input(filename_part, &file_count);
            
            if (file_count != 1) {
                // Multiple filenames after '>' is an error
                free_token_array(file_tokens);
                print_error();
                continue;
            }
            
            // Save the filename
            output_file = strdup(file_tokens[0]);
            free_token_array(file_tokens);
            redirect_output = 1;
            
        } else {
            // No redirection
            command_part = clean_input;
        }
        
        
        // Parse the command into tokens
        int num_tokens = 0;
        char **command_tokens = parse_input(command_part, &num_tokens);
        
        // If no tokens, nothing to do
        if (num_tokens == 0) {
            free_token_array(command_tokens);
            if (output_file) {
                free(output_file);
            }
            continue;
        }
        
        
        // Check if it's a built-in command
        
        // Built-in: exit
        if (strcmp(command_tokens[0], "exit") == 0) {
            
            // exit should have no arguments
            if (num_tokens != 1) {
                print_error();
                free_token_array(command_tokens);
                if (output_file) {
                    free(output_file);
                }
                continue;
            }
            
            // Clean up everything and exit
            free_token_array(command_tokens);
            if (output_file) {
                free(output_file);
            }
            for (int i = 0; i < num_paths; i++) {
                free(search_path[i]);
            }
            free(search_path);
            free(input_line);
            exit(0);
        }
        
        
        // Built-in: cd (change directory)
        else if (strcmp(command_tokens[0], "cd") == 0) {
            
            // cd needs exactly one argument (the directory)
            if (num_tokens != 2) {
                print_error();
                free_token_array(command_tokens);
                if (output_file) {
                    free(output_file);
                }
                continue;
            }
            
            // Try to change directory
            if (chdir(command_tokens[1]) != 0) {
                // chdir failed (directory doesn't exist, etc.)
                print_error();
            }
            
            free_token_array(command_tokens);
            if (output_file) {
                free(output_file);
            }
            continue;
        }
        
        
        // Built-in: path (set search path for commands)
        else if (strcmp(command_tokens[0], "path") == 0) {
            
            // Free the old path
            for (int i = 0; i < num_paths; i++) {
                free(search_path[i]);
            }
            free(search_path);
            
            // path with no arguments = empty path
            if (num_tokens == 1) {
                search_path = malloc(0);
                num_paths = 0;
            } else {
                // Set new path from arguments
                num_paths = num_tokens - 1;
                search_path = malloc(num_paths * sizeof(char*));
                if (search_path == NULL) {
                    print_error();
                    exit(1);
                }
                
                for (int i = 0; i < num_paths; i++) {
                    search_path[i] = strdup(command_tokens[i + 1]);
                }
            }
            
            free_token_array(command_tokens);
            if (output_file) {
                free(output_file);
            }
            continue;
        }
        
        
        // Not a built-in command, so try to execute it
        
        // If path is empty, can't run any commands
        if (num_paths == 0) {
            print_error();
            free_token_array(command_tokens);
            if (output_file) {
                free(output_file);
            }
            continue;
        }
        
        // Search for the command in the path directories
        char *executable_path = NULL;
        char *cmd_name = command_tokens[0];
        
        for (int i = 0; i < num_paths; i++) {
            // Make full path (e.g., /bin/ls)
            char *try_path = make_full_path(search_path[i], cmd_name);
            
            // Check if file exists and is executable
            if (access(try_path, X_OK) == 0) {
                executable_path = try_path;
                break;  // found it!
            }
            
            free(try_path);  // didn't find it in this directory
        }
        
        // If command not found in any directory, it's an error
        if (executable_path == NULL) {
            print_error();
            free_token_array(command_tokens);
            if (output_file) {
                free(output_file);
            }
            continue;
        }
        
        
        // Fork a child process to run the command
        pid_t child_pid = fork();
        
        if (child_pid < 0) {
            // fork() failed
            print_error();
            free(executable_path);
            free_token_array(command_tokens);
            if (output_file) {
                free(output_file);
            }
            continue;
            
        } else if (child_pid == 0) {
            // This is the child process
            
            // Handle output redirection if needed
            if (redirect_output) {
                // Open the output file
                int file_descriptor = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                
                if (file_descriptor < 0) {
                    // Couldn't open file
                    print_error();
                    _exit(1);
                }
                
                // Redirect stdout to the file
                if (dup2(file_descriptor, STDOUT_FILENO) < 0) {
                    print_error();
                    close(file_descriptor);
                    _exit(1);
                }
                
                close(file_descriptor);
            }
            
            // Build argv array for execv
            // Need to set argv[0] to the full path so error messages show correct program name
            int argc = 0;
            while (command_tokens[argc] != NULL) {
                argc++;
            }
            
            char **exec_argv = malloc((argc + 1) * sizeof(char*));
            if (exec_argv == NULL) {
                print_error();
                _exit(1);
            }
            
            // Set argv[0] to full path (e.g., /bin/ls instead of just ls)
            exec_argv[0] = executable_path;
            // Copy remaining arguments
            for (int i = 1; i < argc; i++) {
                exec_argv[i] = command_tokens[i];
            }
            exec_argv[argc] = NULL;
            
            // Execute the command
            // execv replaces this process with the new program
            execv(executable_path, exec_argv);
            
            // If execv returns, something went wrong
            print_error();
            free(exec_argv);
            _exit(1);
            
        } else {
            // This is the parent process
            
            // Wait for child to finish
            int child_status;
            waitpid(child_pid, &child_status, 0);
            
            // Clean up
            free(executable_path);
            free_token_array(command_tokens);
            if (output_file) {
                free(output_file);
            }
        }
        
    }  // end of main while loop
    
    
    // Clean up before exiting (when EOF is reached)
    for (int i = 0; i < num_paths; i++) {
        free(search_path[i]);
    }
    free(search_path);
    free(input_line);
    
    return 0;
}