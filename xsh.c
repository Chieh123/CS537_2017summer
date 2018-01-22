#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

char *cwd = NULL;
char *path[128];
int num_path = 0;
// path[0] should always be /bin


void print_error_message()
{
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
    fflush(stderr);
    return;
}

int add_to_path(char* s)
{
    if ( s == NULL ) {
        return num_path;
    }
    path[num_path] = malloc((strlen(s)+1)*sizeof(char));
    strcpy(path[num_path], (const char*)s);
    num_path++;
    return num_path;
}

void free_path()
{
    int i;
    int n = add_to_path(NULL);

    for (i = 0; i < n; i++) {
        free(path[i]);
    }
    num_path = 0;
    return;
}

void free_opts(char** opts, int num_opts)
{
    for ( int i = 0; i < num_opts; i++ ) {
        free(opts[i]);
    }
    return;
    free(opts);
}

int my_cd(char** commands)
{
    int status = 0;
    char tmp[128];
    if ( commands[2] != NULL ) {
        print_error_message();
        exit(0);
    }


    if ( commands[1] == NULL ||  strcmp(commands[1], "") == 0 ) {
        // change to home directory
        char *home = getenv("HOME");
        status = chdir(home);
    } else {
        status = chdir(commands[1]);
    }
    cwd = getcwd(tmp, 128);
    if ( status != 0 ) {
        status = 1;
    }
    return status;
}

int my_path(char **commands)
{
    int status = 0;
    int i = 0;
    int n;

    if ( commands[1] == NULL ) {
        // list all paths
        n = add_to_path(NULL);
        for ( i = 0; i < n; i++ ) {
            printf("%s\n", path[i]);
        }
    } else {
        // add to path
    	    free_path();//try
        i = 1;
        while ( commands[i] != NULL ) {
            add_to_path(commands[i]);
            i++;
        }
    }
    return status;
}

char* search_execute_command_by_path(char *command)
{
    char *command_path = malloc(sizeof(char)*256);
    struct stat buf;

    for ( int i = 0; i < num_path; i++ ) {
        strcpy(command_path, path[i]);
        if ( command_path[strlen(command_path)] != '/' ) {
            strcat(command_path, "/");
        }
        strcat(command_path, command);
        //printf("The command search path is %s\n", command_path);
        if ( stat(command_path, &buf) == 0 ) {
            return command_path;
        }
    }
    free(command_path);//add
    return NULL;
}

int my_type(char** commands)
{
    int n;
    int i;
    char* command_path;

    if ( commands[2] != NULL ) {
        print_error_message();
        exit(0);
    }

    if ( (strcmp(commands[1], "exit") == 0) || (strcmp(commands[1], "cd") == 0) || (strcmp(commands[1], "path") == 0) || (strcmp(commands[1], "type") == 0) ) {
        printf("%s is a shell builtin\n");
        return 0;
    } else {
        command_path = search_execute_command_by_path(commands[1]);
        if ( command_path == NULL )
            return 1;
        else {
            printf("%s is %s\n", commands[1], command_path);
	    free(command_path);
            return 0;
        }
    }
}

void print_working_directory()
{
    char tmp[256];
    //if ( cwd == NULL ) {
    //    cwd = getenv("PWD");
    //} else {
        cwd = getcwd(tmp, 256);
    //}
    printf("[%s]\n", cwd);
}

void print_shell_prompt(int status)
{
    print_working_directory();
    printf("%d> ", status);
    fflush(stdout);
    return;
}



char** get_args(char *line)
{
    int i = 0;
    char **tmp;
    char *pch;

    tmp = malloc( sizeof(char*) * 128 );
    pch = strtok(line, " \t");
    while ( pch != NULL ) {
        tmp[i] = malloc( sizeof(char) * (1+strlen(pch)));
        strcpy(tmp[i], pch);
        i++;

        //printf("pch %d: %s\n", i,pch);
        pch = strtok(NULL, "  \t");
    }
    tmp[i] = NULL;
    return tmp;
}


int main(int argc, char** argv)
{
    int index_argc = 1;//dofferent with 0027 no need
    int max_input_len = 128;//different with 0027
	int prev_exit_status = 0;
    size_t INPUT_MAX = 128;
    char* line = NULL;
    char** raw_command_token;
    int num_tokens = 0;
    int is_path = 0;
    char *exec_command_path;
    char redirect_file[128];

    // redirect-related variables
    int is_output_redirect = 0;
    int is_herestring = 0;
    char* command_and_args[128];
    char* output_redirect_args[128];
    char* herestring_redirect_args[128];
    int num_command_and_args = 0;
    int num_output_redirect_args = 0;
    int num_herestring_redirect_args = 0;
    int after_output_redirect_symbol = 0;
    int after_herestring_redirect_symbol = 0;
    // init path var
    add_to_path("/bin");

    while ( 1 ) {
        // clear opts
        // reset flags: is_redirect
	if(argc > 1){
		print_error_message();
                free(line);//add
                free_path();//add
		exit(1);
	}
        raw_command_token = NULL;
        num_tokens = 0;
        is_output_redirect = 0;
        is_herestring = 0;
        num_output_redirect_args = 0;
        num_herestring_redirect_args = 0;
        num_command_and_args = 0;

        print_shell_prompt(prev_exit_status);

        int nbytes;
        nbytes = getline(&line, &(INPUT_MAX), stdin);

        if (nbytes > max_input_len+1) {//not include '\n'
            print_error_message();
            prev_exit_status = 1;
            continue;
        }

        if ( strcmp(line, "\n") == 0) {
            continue;
        }

        // parse input
        raw_command_token = get_args(line);

        for ( int i = 0; ; i++ ) {
            if ( raw_command_token[i] == NULL ) {
                break;
            }
            if ( strcmp(raw_command_token[i], ">") == 0 ) {
                is_output_redirect = 1;
            } else if ( strcmp(raw_command_token[i], "<<<") == 0 ) {
                is_herestring = 1;
            } else {
            }
            num_tokens++;
        }

        // only preserve \n for <<< redirect
        if ( is_herestring == 0 ) {
            // the \n is seperate by space
            if ( strcmp(raw_command_token[num_tokens-1], "\n") == 0 ) {
                raw_command_token[num_tokens-1] = NULL;
            } else {
                int last_arg_len = strlen(raw_command_token[num_tokens-1]);
                raw_command_token[num_tokens-1][last_arg_len-1] = '\0';
            }
        }
        if ( raw_command_token[0] == NULL ) {
            continue;
        }
        //for ( int i = 0; i < num_tokens; i++ ) {
            //printf("raw command token[%d] is %s\n", i, raw_command_token[i]);
        //}

        // parse input done

        // check if is builtin or execv file or need redirect
        if ( strcmp(raw_command_token[0], "cd") == 0 ) {
            prev_exit_status = my_cd(raw_command_token);
            if ( prev_exit_status != 0 )
                print_error_message();

        } else if ( strcmp(raw_command_token[0], "type") == 0 ) {
            prev_exit_status = my_type(raw_command_token);
            if ( prev_exit_status != 0 )
                print_error_message();

        } else if ( strcmp(raw_command_token[0], "path") == 0 ) {
            is_path = 1;
            prev_exit_status = my_path(raw_command_token);

        } else if ( strcmp(raw_command_token[0], "exit") == 0 ) {
                free_opts(raw_command_token, num_tokens);//add
		free(line);//add
                free_path();//add
		
            exit(0);

        } else if ( is_output_redirect == 1 ) {
           // Need redirect
           pid_t redirect_pid;

           // classify and check redirect parameters
           for ( int i = 0; i < num_tokens; i++ ) {
               if ( (strcmp(raw_command_token[i], ">") == 0) ) {
                   after_output_redirect_symbol = 1;
                   continue;
               } else if ( strcmp(raw_command_token[i], "<<<") == 0 ) {
                   after_herestring_redirect_symbol = 1;
                   continue;
               }

               if ( after_herestring_redirect_symbol == 1 ) {
                   herestring_redirect_args[num_herestring_redirect_args] = raw_command_token[i];
                   num_herestring_redirect_args++;
                   after_herestring_redirect_symbol = 0;
               } else if ( after_output_redirect_symbol == 1 ) {
                   output_redirect_args[num_output_redirect_args] = raw_command_token[i];
                   num_output_redirect_args++;
                   after_output_redirect_symbol = 0;
               } else {
                   command_and_args[num_command_and_args] = raw_command_token[i];
                   num_command_and_args++;
               }
           }
           if ( num_herestring_redirect_args > 1 || num_output_redirect_args > 1 ) {
               print_error_message();
                free_opts(raw_command_token, num_tokens);//add
		free(line);//add
                free_path();//add
//               exit(1);
               continue;
           }
           if(is_herestring == 1) {
        	   	   pid_t herestring_redirect_pid;
        	   	   int pipefd[2];
        	   	   if ( pipe(pipefd) == -1 ) {
                free_opts(raw_command_token, num_tokens);//add
		free(line);//add
                free_path();//add
        	   	        exit(EXIT_FAILURE);
        	   	    }
        	   	    if ( (herestring_redirect_pid = fork()) < 0 ) {
                free_opts(raw_command_token, num_tokens);//add
		free(line);//add
                free_path();//add
        	   	         exit(EXIT_FAILURE);
        	   	     } else if ( herestring_redirect_pid == 0 ) {
        	   	          // child read from pipe

        	   	       strcpy(redirect_file, output_redirect_args[0]);
        	   	                      strcat(redirect_file, ".out");
        	   	                      int fd_out = open(redirect_file, O_CREAT | O_RDWR , S_IRUSR | S_IWUSR);
        	   	                      strcpy(redirect_file, output_redirect_args[0]);
        	   	                      strcat(redirect_file, ".err");
        	   	                      int fd_err = open(redirect_file, O_CREAT | O_RDWR , S_IRUSR | S_IWUSR);

        	   	                      dup2(fd_out, 1);
        	   	                      dup2(fd_err, 2);
        	   	                      dup2(pipefd[0], 0);
        	   	                      close(pipefd[0]);
        	   	                      close(pipefd[1]);
        	   	                      close(fd_out);
        	   	                      close(fd_err);
        	   	                      exec_command_path = search_execute_command_by_path(command_and_args[0]);
        	   	                      if ( exec_command_path == NULL ) {
        	   	                          print_error_message();
        	   	                          //free(exec_command_path);
                free_opts(raw_command_token, num_tokens);//add
		free(line);//add
                free_path();//add
        	   	                              exit(1);
        	   	                      }
        	   	                      // the output filename is no longer required since we read it from dup, so this last arg have to be set to NULL
        	   	                      //command_and_args[num_command_and_args] = NULL;

        	   	       		free(raw_command_token[0]);
        	   	       		raw_command_token[0] = malloc(sizeof(exec_command_path));
        	   	       		strcpy(raw_command_token[0],exec_command_path);
        	   	                      execv(exec_command_path, command_and_args);
        	   	     } else {
        	   	    	 	  close(pipefd[0]);
        	   	    	      write(pipefd[1], herestring_redirect_args[0], strlen(herestring_redirect_args[0]));
        	   	    	      close(pipefd[1]);
        	   	    	      wait(&prev_exit_status);
        	   	    		  if (WIFEXITED(prev_exit_status)) {
        	   	    	           prev_exit_status = WEXITSTATUS(prev_exit_status);
        	   	    	      }
        	   	     }

           }

           // output redirect by dup2
           else {
        	   if ((redirect_pid = fork()) < 0) {
                free_opts(raw_command_token, num_tokens);//add
		free(line);//add
                free_path();//add
               exit(EXIT_FAILURE);
           } else if (redirect_pid == 0) {
               // create .out and .err file
               strcpy(redirect_file, output_redirect_args[0]);
               strcat(redirect_file, ".out");
               int fd_out = open(redirect_file, O_CREAT | O_RDWR , S_IRUSR | S_IWUSR);
               strcpy(redirect_file, output_redirect_args[0]);
               strcat(redirect_file, ".err");
               int fd_err = open(redirect_file, O_CREAT | O_RDWR , S_IRUSR | S_IWUSR);

               if(fd_out < 0 || fd_err < 0 ){
                   //dup2(fd_err, "An error has occured");
                   print_error_message();
                free_opts(raw_command_token, num_tokens);//add
		free(line);//add
                free_path();//add
                   exit(1);
               }

               dup2(fd_out, 1);
               dup2(fd_err, 2);
               close(fd_out);
               close(fd_err);
               exec_command_path = search_execute_command_by_path(command_and_args[0]);
               if ( exec_command_path == NULL ) {
                   print_error_message();
                   //free(exec_command_path);
                free_opts(raw_command_token, num_tokens);//add
		free(line);//add
                free_path();//add
                       exit(1);
               }
               // the output filename is no longer required since we read it from dup, so this last arg have to be set to NULL
               //command_and_args[num_command_and_args] = NULL;

		free(raw_command_token[0]);
		raw_command_token[0] = malloc(sizeof(exec_command_path));
		strcpy(raw_command_token[0],exec_command_path);
               execv(exec_command_path, command_and_args);
           } else {
                //printf("This is parent\n");
                wait(&prev_exit_status);
                if (WIFEXITED(prev_exit_status)) {
                        prev_exit_status = WEXITSTATUS(prev_exit_status);
                }
                //printf("Wait success: status is %d\n", prev_exit_status);
           }
           }
        } else if ( is_herestring == 1 ) {
                pid_t herestring_redirect_pid;
                after_herestring_redirect_symbol = 0;
                num_herestring_redirect_args =  0;
                // classify and check redirect parameters
                for ( int i = 0; i < num_tokens; i++ ) {
                    if ( strcmp(raw_command_token[i], "<<<") == 0 ) {
                        after_herestring_redirect_symbol = 1;
                        continue;
                    }
                    if ( after_herestring_redirect_symbol == 0 ) {
                        command_and_args[num_command_and_args] = raw_command_token[i];
                        num_command_and_args++;
                    } else if ( strcmp(raw_command_token[i], "\n") != 0){
                        herestring_redirect_args[num_herestring_redirect_args] = raw_command_token[i];
                        num_herestring_redirect_args++;
                    }
                }

//printf("num_herestring_redirect_args : %d- %s -\n", num_herestring_redirect_args, raw_command_token[num_tokens - 1]);
                if ( num_herestring_redirect_args != 1 ) {
                    print_error_message();
                   // exit(1);
                   // continue;
                }

                // herestring redirect from shell by pipe
                // pipefd[0] read, pipefd[1] write
                int pipefd[2];
                if ( pipe(pipefd) == -1 ) {
                free_opts(raw_command_token, num_tokens);//add
		free(line);//add
                free_path();//add
                    exit(EXIT_FAILURE);
                }
                if ( (herestring_redirect_pid = fork()) < 0 ) {
                free_opts(raw_command_token, num_tokens);//add
		free(line);//add
                free_path();//add
                    exit(EXIT_FAILURE);
                } else if ( herestring_redirect_pid == 0 ) {
                    if ( num_herestring_redirect_args != 1 ) { 
                free_opts(raw_command_token, num_tokens);//add
		free(line);//add
                free_path();//add
exit(1); 
// for badherestr2

}
                //    if (strcmp(raw_command_token[num_tokens-1], "<<<") == 0) exit(1); // for badherestr1
                    

                    // child read from pipe
                    dup2(pipefd[0], 0);
                    close(pipefd[0]);
                    close(pipefd[1]);

                    exec_command_path = search_execute_command_by_path(command_and_args[0]);
                    if ( exec_command_path == NULL ) {
                        print_error_message();
                        //free(exec_command_path);
                free_opts(raw_command_token, num_tokens);//add
		free(line);//add
                free_path();//add
                        exit(1);
                    }
                    // the input part is no longer required since we'll read it from pipe, so we can (we have to) set this to NULL
                    //command_and_args[num_command_and_args] = NULL;

		    free(raw_command_token[0]);
		    raw_command_token[0] = malloc(sizeof(exec_command_path));
		    strcpy(raw_command_token[0],exec_command_path);
                    execv(exec_command_path, command_and_args);
                } else {
                    close(pipefd[0]);
                    write(pipefd[1], herestring_redirect_args[0], strlen(herestring_redirect_args[0]));
                    close(pipefd[1]);
                    wait(&prev_exit_status);
		   // if (WIFEXITED(prev_exit_status)) {
                        prev_exit_status = WEXITSTATUS(prev_exit_status);
                   // }
                   // printf("Wait success: status is %d\n", prev_exit_status);
                }
        } else {
            // execv
            pid_t pid;
            if ((pid = fork()) < 0) {
                free_opts(raw_command_token, num_tokens);//add
		free(line);//add
                free_path();//add
                exit(EXIT_FAILURE);
            } else if (pid == 0) {
                exec_command_path = search_execute_command_by_path(raw_command_token[0]);
                if ( exec_command_path == NULL ) {

                    print_error_message();
                free_opts(raw_command_token, num_tokens);//add
		free(line);//add
                free_path();//add
                    //free(exec_command_path);
                    exit(1);
                }

		free(raw_command_token[0]);
		raw_command_token[0] = malloc(sizeof(exec_command_path));
		strcpy(raw_command_token[0],exec_command_path);
                execv(exec_command_path, raw_command_token);

            } else {
                waitpid(pid, &prev_exit_status, 0);
                if (WIFEXITED(prev_exit_status)) {
                        prev_exit_status = WEXITSTATUS(prev_exit_status);
                }
                //printf("Wait success: status is %d\n", prev_exit_status);
            }
        }
    free_opts(raw_command_token, num_tokens);
    }
    free(line);
    //if ( is_path == 1 ) {
        free_path();
   // }
	return 0;
}

