#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

int background = 1; //background mode toggle
int background_process[50]; //holds Background processes
int num_background_process = 0; //holds number of background process
void command_status(int, int); //by doing this I can organize my code better

//-----------------------Redirect--------------------------//
//redirects stdin or stdout to a file
void redirect(char * file_name, int redirect_location){
  //open file and set to close from exec
  int file;
  //opens for write/read dependent on redirect location
  if(redirect_location == 0){
    file = open(file_name, O_RDONLY);
  }else{
    file = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  }
  fcntl(file, F_SETFD, FD_CLOEXEC); //sets to close on exec close
  if(file == -1){ perror(file_name); exit(1); }

  fflush(stdout);
  fflush(stdin);

  int result = dup2(file, redirect_location); //actual redicrection happens
  if(result == -1) { perror("dup2"); exit(2); }
}

//checks if redirection is needed and manimulates the command accordily
char **is_redirected(char **exe){
  int i, j;
  char **exe2 = malloc(sizeof(char*) * 512);
  //runs through each argument looking for redicrection
  for(i = 0, j = 0; exe[i] != NULL; i++, j++){
    if(!strcmp(exe[i], ">") && exe[i+1] != NULL && exe[i+2] == NULL){
      //redirect output
      redirect(exe[i+1], 1);

      //gets rid of the > and filename for exec
      exe2[j] = NULL;
      exe2[j+1] == NULL;
      i++; j++;
    }else if(!strcmp(exe[i], "<") && exe[i+1] != NULL){
      //redirect Input
      redirect(exe[i+1], 0);

      //gets rid of the > and filename for exec
      exe2[j] = NULL;
      exe2[j+1] == NULL;
      i++; j++;
    }else{
      exe2[i] = exe[i]; //otherwise just swps from one to the other
    }
  }
  exe2[j] = NULL;
  return exe2;
}

//returns stdin and stdout to their defaults
void reset_redirection(int stdout_save, int stdin_save){
  //reset stdout
  fflush(stdout);
  int result = dup2(stdout_save, 1);
  if(result == -1) { perror("dup2"); exit(2); }

  //reset stdin
  fflush(stdin);
  result = dup2(stdin_save, 0);
  if(result == -1) { perror("dup2"); exit(2); }
}


//-----------------------Background--------------------------//
//removes a process id from the background_process array
int *pop_background_process(int index){
  int i;
  //moves each process back a location to get rid of removed process id
  for(i = index; i < num_background_process-1; i++){
    background_process[i] = background_process[i+1];
  }
  background_process[i] = 0;
}

//checks if any background processes have finished
int check_background(){
  int childExitStatus = -5;
  int i;
  //runs through the numbers of background process to see if they are running
  for(i = 0; i < num_background_process; i++){
    if(waitpid(background_process[i], &childExitStatus, WNOHANG)){
      printf("background pid %i is done: ", background_process[i]);
      fflush(stdout);
      command_status(childExitStatus, 1); //returns the exit status
      pop_background_process(i); //removes the process id from the aray
      num_background_process--;
      i--;
    }
  }
  //kills any process stoped by ^C
  waitpid(-1, &childExitStatus, WNOHANG);
  return childExitStatus;
}

//checks if the child proces is running in the background
int is_background(char **exe, int background){
  int i;
  //looks for the & to tell if this needs to go in the backgorund
  for(i = 0; exe[i] != NULL; i++){
    if(!strcmp(exe[i], "&") && exe[i+1] == NULL){
      if(background){
        num_background_process++;
        return 1;
      }
      break;
    }
  }
  return 0;
}

//looks for & to know if it is a background proces
char **set_background(char **exe, int background){
  int i;
  char **exe2 = malloc(sizeof(char*) * 512);
  //looks for the & and removes it along with setting input and output to null
  for(i = 0; exe[i] != NULL; i++){
    if(!strcmp(exe[i], "&") && exe[i+1] == NULL){
      if(background){
        //so that the process is not seen
        redirect("/dev/null", 1);
        redirect("/dev/null", 0);
      }
      exe2[i] = NULL;
      break; // breaks out of the loop as we know that this is the end
    }
    exe2[i] = exe[i];
  }
  return exe2;
}


//----------------------Get-Input------------------------//
//parses the data from the user as to be easier to work with
char **parse_command(char *raw_input){
  int i, j, k;
  char *token;
  char **exe = malloc(sizeof(char*) * 512);

  //tokenises the data into spereate strings and puts into an array
  token = strtok(raw_input, " ");
  for(i = 0; token != NULL; i++){
    exe[i] = token;

    token = strtok(NULL, " ");
  }
  exe[i] = NULL;
  exe[i+1] = NULL;

  //looks for the $$ to convert to PID
  for(i = 0; i < 512; i++){
    //finds the $$ and makes sure to skip it the first argument is NULL
    if(exe[i] != NULL && strstr(exe[i], "$$") != NULL){
      int before = 1;
      char temp[100]; //holds the new version of the command
      for(j = 0; j < strlen(exe[i]); j++){
        if(exe[i][j] == '$' && exe[i][j+1] == '$' ){
          char pid[10];
          sprintf(pid, "%i", getpid());
          fflush(stdout);
          for(k = j; k < strlen(pid)+j; k++){//makes it so the PID can be added in smoothly
            temp[k] = pid[k - j];
          }
          before--;
        }else{
          //takes account weather the PID has been added or not
          if(before){
            temp[j] = exe[i][j];
          }else{
            temp[j+k-1] = exe[i][j+1];
          }
        }
      }
      strcpy(exe[i], temp);
    }
    //changes any blank commands to "" to be easier to deal with later
    if(exe[i] == NULL){
      if(i < 1){
        exe[0] = "";
        break;
      }
      //changes the last newline to a null terminator
      for(j = 0; j < strlen(exe[i-1]); j++){
        if(exe[i-1][j] == '\n'){
          exe[i-1][j] = '\0';
        }
      }
      break;
    }
  }
  return exe;
}

//gets the input from the use and parses it
char **get_command(int stdout_save, int stdin_save){
  size_t size = 2048;
  char *raw_input = (char *)malloc(size * sizeof(char));
  raw_input[0] = '\n';
  reset_redirection(stdout_save, stdin_save); //resets the redirection before taking in any commands

  //could use this to display the dir like a real shell
  char cwd[256];
  getcwd(cwd, sizeof(cwd));

  //makes the shell look like a shell
  printf("%s:", cwd);
  fflush(stdout);

  //gets the input
  fgets(raw_input, size, stdin);
  fflush(stdin);

  //parses the input into an array
  char **exe = parse_command(raw_input);

  return exe;
}


//---------------------Commands-------------------------//
//kill all running processes and the shell itself
void command_exit(){ 
  int i;
  //kills all background process before exiting
  for(i = 0; i < num_background_process; i++){
      kill(background_process[i], SIGKILL);
  }
  exit(0);
}

//change the directory to specified path
void command_cd(char **exe){
  //sets the directory to home if a path is not provided
  if(exe[1] == NULL){
      chdir(getenv("HOME"));
  }else{
    chdir(exe[1]);
  }
}

//states the status of the current runing process
void command_status(int exitMethod, int normal_exit){
  
  //checks if killed by a signal
  if(WIFSIGNALED(exitMethod) != 0){
    printf("terminated by signal %i\n", WTERMSIG(exitMethod));
    fflush(stdout);
  }else if(normal_exit){ //lets choose if to show good exit values
      printf("exit value %i\n", WEXITSTATUS(exitMethod));
      fflush(stdout);
  }
}

//sends all other commands to be ran by exec
int command_other(char **exe_st){
  //stores stdin and stdout as to reset them later
  int stdin_save = dup(0);
  int stdout_save = dup(1);

  pid_t spawnpid = -5;
  int childExitStatus = -5;
  int last_exit_status = -5; //holds the last process exit statis as to display when statis is called

  char **exe;//holds the commands

  //makes a clone of the process
  spawnpid = fork();
  switch(spawnpid) {
    case -1: //if the fork fails
      perror("Fork Failed...");
      exit(1);
      break;
    case 0: //what the child process will do
      exe = is_redirected(exe_st);
      if(exe[0][0] == '#'){exe[0] = "";} //ignores comments
      //takes account hitting only enter and ^C
      if(strcmp(exe[0], "")){
        execvp(exe[0], set_background(exe, background)); //the child process transforms into the command it was given
        perror(exe[0]);
      }
      //if exec fails this will close it
      command_exit();
      break;
    default: ; //what the parent does
      reset_redirection(stdout_save, stdin_save);
      //ignores wait if is a background process
      if(!is_background(exe_st, background)){
        waitpid(spawnpid, &childExitStatus, 0); //the parent waits for the child to finish running
        command_status(childExitStatus, 0); //outputs output status if bad
      }else{
        //shows that proces has been put into the background
        printf("background pid is: %i\n", spawnpid);
        fflush(stdout);
        background_process[num_background_process-1] = spawnpid;
      }
      break;
  }
  //gets the last exit status from the last background process to be killed
  last_exit_status = check_background();
  //if the last_exit_status was not gotten from a background process
  if(last_exit_status == -5){
    last_exit_status = childExitStatus;
  }
  return last_exit_status;
}


//----------------------Signals------------------------//
// sigint is ignored for the shell and a message is sent
void catchSIGINT(int signo){
  //makes the command look more clean
  write(STDOUT_FILENO, "\n", 1);
}

// sigtstp is set to swap background mode
void catchSIGTSTP(int signo){
  //checks the background mode and swaps it
  if(background){
    write(STDOUT_FILENO, "\nEntering foreground-only mode (& is now ignored)\n", 50);
    background = 0;
  }else{
    write(STDOUT_FILENO, "\nExiting foreground-only mode\n", 30);
    background = 1;
  }
}

// catches the signals when they come
void signal_catch(){
    struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0};

    //sets up the signal handler for ^C
    SIGINT_action.sa_handler = catchSIGINT;
    sigaction(SIGINT, &SIGINT_action, NULL);

    //sets up the signal hangler for ^Z
    SIGTSTP_action.sa_handler = catchSIGTSTP;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

}


//-----------------------Main--------------------------//
int main(){
  int exitStatus = 0;
  int i = 0;
  int stdin_save = dup(0);
  int stdout_save = dup(1);
  signal_catch(); //catches the signals

  //keeps looking tell exit is called
  while(1){
    //takes in command and parses it
    char **exe = get_command(stdout_save, stdin_save);
    //checks for built in commands otherwise sends to exec
    if(!strcmp(exe[0], "exit")){
      command_exit();
    }else if(!strcmp(exe[0], "cd")){
      command_cd(exe);
    }else if(!strcmp(exe[0], "status")){
      command_status(exitStatus, 1);
    }else{
      exitStatus = command_other(exe); //sends command to exec
    }
    free(exe);//frees the command
  }
  return 0;
}
