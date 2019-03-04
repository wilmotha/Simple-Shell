import os
import sys

#-----------------------Redirect-------------------------#
def redirect(file_name, redirect_location):
    if redirect_location == 0:
        file = os.open(file_name, O_RDONLY)
    else:
        file = os.open(file_name, os.O_WRONLY | os.O_CREAT | os.O_TRUNC) #0644
    os.fcntl(file, os.F_SETFD, os.FD_CLOSEXEC)
    if file == -1:
        os.perror("dup2")
        exit(1)

    sys.stdout.flush()
    sys.stdin.flush()

    result = dup2(file, redirect_location)
    if file == -1: 
        os.perror("dup2")
        exit(2)

def is_redirected(exe):
    for i in range(len(exe)):
        if exe[i] == ">" and i+2 < len(exe):
            redirect(exe[i+1], 1)
            exe.pop(i)
            exe.pop(i+1)
            break
        elif exe[i] == "<" and i+1 < len(exe):
            redirect(exe[i+1], 0)
            exe.pop(i)
            exe.pop(i+1)
            break
    return exe

def reset_redirection(stdout_save, stdin_save):
    sys.stdout.flush()
    result = os.dup2(stdout_save, 1)
    if result == -1:
        os.perror("dup2")
        exit(2)

    sys.stdin.flush()
    result = os.dup2(stdin_save, 0)
    if result == -1:
        os.perror("dup2")
        exit(2)


#---------------------Background------------------------#


#----------------------Get-Input------------------------#
def get_command(stdout_save, stdin_save):
        reset_redirection(stdout_save, stdin_save)
        return input(os.path.abspath(os.getcwd()) + ": ").split()


#----------------------Commands-------------------------#
def command_exit():
    exit()

def command_cd(exe):
    if len(exe) == 1:
        os.chdir(os.path.expanduser("~"))
    else:
        os.chdir(exe[2])

def command_status():
    print("NOTHING TO SEE HERE")

def command_other(exe):
    stdin_save = os.dup(0)
    stdout_save = os.dup(1)

    childExitStatus = -5

    spawnpid = os.fork()
    if spawnpid == -1:
        print("Error")
    elif spawnpid == 0:
        sys.stdout.flush()
        os.execvp(exe[0], exe)
        exit()
    else:
        reset_redirection(stdout_save, stdin_save)
        os.waitpid(spawnpid, 0)


#----------------------Signals------------------------#


#-----------------------Main--------------------------#
def main():
    exitStatus = 0
    stdin_save = os.dup(0)
    stdout_save = os.dup(1)

    loop = 0
    while(loop < 4):
        exe = get_command(stdin_save, stdin_save);
        if exe[0] == "exit":
            command_exit()
        elif exe[0] == "cd":
            command_cd(exe)
        elif exe[0] == "status":
            command_status()
        else:
            command_other(exe)
            loop != 1

main()
