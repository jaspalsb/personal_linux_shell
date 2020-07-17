/******************************************************************************
Jaspal Singh Bainiwal
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
//problem with the sig handler function is that I can't pass my own argments to the function
//I learned from here https://stackoverflow.com/questions/6970224/providing-passing-argument-to-signal-handler
//That the best method is to instead use a gloabl variable instead of the parameters
//hence why I have intialized my ctrl_z_command flag set to zero which means background and foreground mode enabled
int CTRL_Z_command = 0;
//using the lecture video 3.3 signals as guide to help me create the SIGSTP signal to
//determine foreground or background mode. According to the lecture the SIGSTOP is not catchable
//with sigstp with ctrl z is mapped to that one so I will use that signal.
void catchSIGTSTP(int signo)
{
  //if the ctrlz command is 0 then we are in all modes so change to 1 so it will only be foreground mode
  //signo will tell us the signal number however in the rpgoram specs its not needed for reading also
  //I don't think sigstp has a number according to lecture so it might be useless.
  if (CTRL_Z_command == 0)
  {
    //like in lecture 3.3 around 52 minute mark I will also make string with the message used in program specs
    char* message = "\nEntering foreground-only mode (& is now ignored)\n";
    //according to the lecture we have to use write instead of printf because this is non-rentrant
    //annoying have to count the chars which were 50
    write(STDOUT_FILENO, message, 50);
    CTRL_Z_command = 1;
    //printf(": ");
    //fflush(stdout);
  }
  else
  {
    char* message = "\nExiting foreground-only mode\n";
    write(STDOUT_FILENO, message, 30);
    CTRL_Z_command = 0;
    //printf(": ");
    //fflush(stdout);
  }
}
/****************************************************************************************
This is for my SIGINT handle for the child process to be killed and have the message displayed
that was given in the example for the program specs.
********************************************************************************************/
void catchSIGINT(int signo)
{
  char *message = "terminated by signal 2\n";
  write(STDOUT_FILENO, message, 23);
}
int main()
{
  //this is the flag that keeps the smallsh program running in a while loop
  int smallShell = 1;
  //will be used in strtok to count number of arguments passed by the command prompt
  int commandCounter;
  //an array of char pointers which can hold 512 arguments
  char *eachCommand[512];
  //like in lecture 3.1 I created the childexit integer which I can use to analyze the exit status for my status command
  int childExitMethod = 2;
  //these are my flags which will help me to understand which type of command the user has entered
  int commentFLAG, redirectionFLAG, lessThanFLAG, greaterThanFLAG, backgroundFLAG, newlineFLAG;
  //the program specs said to store the child process id in an array so we can do background clean up
  int backgroundCheckID [512];
  //also need a background index checker which will increment everytime a new background process has been forked
  int backgroundIndexer = 0;
  int backgroundIndexerLooper;
  while (smallShell == 1)
  {
    /***********************************************************************************************************
    First thing first need to handle the sigstp because that can be sent at anytime so better to set up first.
    I will follow the lecture 3.3 around 47 minute mark to setup the SIGSTP handler
    ************************************************************************************************************/
    //set entire struct to be cleared by settign to zero
    struct sigaction SIGSTOP_action = {0}, ignore_action = {0};
    //set handler to be the function catchstp that I wrote
    SIGSTOP_action.sa_handler = catchSIGTSTP;
    //this will block/delay all signals
    sigfillset(&SIGSTOP_action.sa_mask);
    //to counter the strange getline seg fault I changed the sa flag to restart which helped
    //https://stackoverflow.com/questions/19140892/strange-sigaction-and-getline-interaction
    SIGSTOP_action.sa_flags = SA_RESTART;
    //now this will register the signal when ever the signSTP signal is sent the sigstop struc that I created
    //will be run as I have it set up.
    sigaction(SIGTSTP, &SIGSTOP_action, NULL);
    /**********************************************************************************************************
    Now I also need to setup the sigin signal as well. The program specs specify that when the shell is running a
    sigint will not kill the shell. TO do this I have to setup sigin to be ingnored like in the lecture video 3.3
    In that video the ignore action sigaction struct was setup so I will do the same for the sigint. To ignore I will
    have to setup the ignore_action struct to be set to SIG_IGN.
    ***********************************************************************************************************/
    ignore_action.sa_handler = SIG_IGN;
    //now establish the sigaction for the SIGINT signal
    sigaction(SIGINT, &ignore_action, NULL);
    /***********************************************************************************************************
    this is the background check for child process. This check is done by looping throught all the values
    stored in my backgroundcheck array. then for each child id that is stored in the array I run the waitpid
    function with that argument passed to it. The way the waitpid with wnohang works is that it returns a > 0 value
    if that child process id has exited and 0 or -1 if no change. learned about it from here
    https://stackoverflow.com/questions/14548367/using-waitpid-to-run-process-in-background
    ************************************************************************************************************/
    int status;
    int cstatus;
    for (backgroundIndexerLooper = 0; backgroundIndexerLooper < backgroundIndexer; backgroundIndexerLooper++)
    {
      //get the status of the waitpid for the child process id passed to it from my array.
      status = waitpid(backgroundCheckID[backgroundIndexerLooper], &cstatus, WNOHANG);
      //now if it is greater than 0 then child has exited
      if (status > 0)
      {
        //now in the program specs we also need to list the exit status which was given to us in the lecture video 3.1
        //I will use that as the template and check for both signal and exit status
        if(WIFEXITED(cstatus) != 0)
        {
          int exitStatus = WEXITSTATUS(cstatus);
          printf("background pid %d is done: exit value %d\n", backgroundCheckID[backgroundIndexerLooper], exitStatus);
          fflush(stdout);
        }
        else if(WIFSIGNALED(cstatus) != 0)
        {
          int termSig = WTERMSIG(cstatus);
          printf("background pid %d is done: terminated by signal %d\n", backgroundCheckID[backgroundIndexerLooper], termSig);
          fflush(stdout);
        }
      }
    }
    //************************************************************************************************************************

    //start the prompt which starts with :
    printf(": ");
    //https://c-for-dummies.com/blog/?p=1112 this helped me learn about getline
    char *user_command = NULL;
    ssize_t bufsize = 2048;
    getline(&user_command, &bufsize, stdin);
    newlineFLAG = 0;
    //get the newline flag set like in the example output of program on program specs page
    //I saw there were newline just entered so this is to handle that case.
    if(user_command[0] == '\n')
    {
      //printf("new line entered in if statement\n");
      newlineFLAG = 1;
    }
    else
    {
      //now I can get the user command and break it by either one of the two delimiters space or new line
      char *tokenArgument = strtok(user_command, " \n");
      commandCounter = 0;
      eachCommand[commandCounter] = tokenArgument;

      while (tokenArgument != NULL)
      {
        //printf("%s ", tokenArgument);
        tokenArgument = strtok(NULL, " \n");
        commandCounter = commandCounter + 1;
        eachCommand[commandCounter] = tokenArgument;
      }
    }
    //set all my flags to zero false
    commentFLAG = 0;
    redirectionFLAG = 0;
    lessThanFLAG = 0;
    greaterThanFLAG = 0;
    backgroundFLAG = 0;
    /********************************************************************************************
    Now in this section of the code I will be working on setting flags for the various commands
    I will parse the eachCommand array and look for key command prompts such as >, <, and #
    If these commands are found I will set the flag for that specific command to be true or in c case
    to be 1.
    *********************************************************************************************/
    int x;
    //https://stackoverflow.com/questions/47734579/first-character-of-a-pointer-to-an-array
    //found out how to get first letter in a string from that source
    if (user_command[0] == '#')
    {
      //printf("I am in comment flag setter\n");
      commentFLAG = 1;
    }
    //neeed to make sure the newline flag was still 0 because if the new line flag was 1
    //that means in this case there weren't any information added to the eachcommand array
    //when there isn't any information in the eachcommand I will get a segmentation fault
    else if (newlineFLAG == 0)
    {
      //how this works is that I check if the foreground mode has been enabled.
      //If it is enabled then I check if the last command the user has sent check if it has the ampersand
      //if it does remove it with null and make the commmand counter one less.
      //by doing this the background flag won't be triggered so then the command will just run in foreground
      if (CTRL_Z_command == 1)
      {
        if (strcmp("&", eachCommand[commandCounter - 1]) == 0)
        {
          //printf("the end had an ampersand so I will remove it\n");
          eachCommand[commandCounter - 1] = '\0';
          commandCounter = commandCounter - 1;
        }
      }
      for (x = 0; x < commandCounter; x++)
      {
        //needed this so I can check if only the last command is an ampersand
        int lastCommandisAmpered = commandCounter - x;
        //printf("lastcommand: %d\n", lastCommandisAmpered);
        if(strcmp("<", eachCommand[x]) == 0)
        {
          redirectionFLAG = 1;
          lessThanFLAG = 1;
        }
        else if(strcmp(">", eachCommand[x]) == 0)
        {
          redirectionFLAG = 1;
          greaterThanFLAG = 1;
        }
        //now if the last command is reached I need to check it
        else if(lastCommandisAmpered == 1)
        {
          //I need to check that the last command is an ampersand if it is then trigger the flag.
          //printf("now testing last command\n");
          if (strcmp("&", eachCommand[x]) == 0)
          {
            //printf("background flag triggered\n");
            backgroundFLAG =1;
          }
        }
      }
      /*********************************************************************************************
      This section will work on the $$ expansion of the PID for the shell.
      I check every command entered by the user and if it has the substring $$.
      my method to solve this was to find substring of the string which would be find $$ in the command string
      to do this found out from stack https://stackoverflow.com/questions/12784766/check-substring-exists-in-a-string-in-c
      this can be done with strstr. With strstr it returns a pointer of where the $$ starts. Then when
      I know that this exists in the command I can start manipulating the string to get the pid of the shell.
      Only when p is not null I go into the if statement to do the manipulation. The manipulation happens
      by null the $$. Once that happens use sprintf to create new string witht eh manipulated holder and
      add the pid to it.
      *********************************************************************************************/
      for (x = 0; x < commandCounter; x++)
      {
        //printf("String len of each command: %d\n", strlen(eachCommand[x]));
        char *holder = eachCommand[x];
        char *p;
        //printf("holder string %s\n", holder);
        //printf("string len of holder: %d\n", strlen(holder));
        p = strstr(holder, "$$");
        if (p)
        {
          //printf("found %s\n", p);
          holder[strlen(holder) - 1] = '\0';
          holder[strlen(holder) - 1] = '\0';
          int shellPID = getpid();
          //learned about sprintf here https://stackoverflow.com/questions/20814675/how-to-useput-sprintf-to-char-pointer-c
          //this will rebuild the command with the $$ in it by adding the two delted $$ in holder plus the shell process id
          //then throw it back into the eachCommand array.
          sprintf(eachCommand[x], "%s%d", holder, shellPID);
          //printf("new holder %s\n", eachCommand[x]);
        }
        /*int eachCharChecker = 0;
        while (eachCharChecker != strlen(holder))
        {
          printf(": %s\n", holder[eachCharChecker]);
          eachCharChecker++;
        }*/

      }
    }

    //checking to make sure my commands are correctly being listed in array format.

    /*printf("now printing command counter list\n");
    for (x = 0; x < 1; x++)
    {
      printf(eachCommand[x]);
    }*/

    //had to have the newline flag first because of segmentation fault
    //I figured the seg fault was maybe due to the comparing of eachcommand strings which
    //never get set in the while loop when a new line is entered so to make sure those compares never happen
    //I have the newline tested first so if it is triggered the rest of the elif will not be executed
    //if there not executed I can avoid the seg fault. However if I had the newline flag as an elif like beofre
    //all my other elif would have to strcmp with null
    if (newlineFLAG == 1)
    {
      //printf("newline entered\n");
      //fflush(stdout);
    }
    else if (strcmp("exit", eachCommand[0]) == 0)
    {
      //printf("exit command\n");
      smallShell = 0;
    }
    else if(strcmp("status", eachCommand[0]) == 0)
    {
      //printf("I am here\n");
      //lecture 3.1 process around 26 minute mark of video
      //the first function wifexited will check if status of laste process was terminated normally
      //to write this function I will use the template given in the lecture
      if(WIFEXITED(childExitMethod) != 0)
      {
        //printf("The process exited normally\n");
        //the actual status value is in the wesistatus macro
        int exitStatus = WEXITSTATUS(childExitMethod);
        printf("exit value %d\n", exitStatus);
        fflush(stdout);
      }
      //now we can also check if the last process was exited with a signal instead
      //to check this the wifisignaled marcro returns a non zero
      //to have the if statement not check both macros the signal check will be in a elif statement
      //also the format is from the lecture 3.1
      else if(WIFSIGNALED(childExitMethod) != 0)
      {
        //printf("process was terminated by a signal\n");
        int termSig = WTERMSIG(childExitMethod);
        printf("terminated by signal %d\n", termSig);
        fflush(stdout);
      }
    }
    else if (strcmp("cd", eachCommand[0]) == 0)
    {
      //in the lecture 3.1 video it talks about getenv which is environment variable.
      //this helped me find out that I can get the home directory varibale while using getenv
      //https://www.tutorialspoint.com/c_standard_library/c_function_getenv
      //now if my command counter is larger than 1 then I know that the cd is to some directory that was specified
      //chdir lets me change directory used this guide https://stackoverflow.com/questions/9493234/chdir-to-home-directory
      if (commandCounter > 1)
      {
        //printf("I am here in more than one command cd\n");
        chdir(eachCommand[1]);
      }
      else
      {
        //printf("in the cd home command\n");
        //lecture 3.1 talked about using this to move around directories
        chdir(getenv("HOME"));
      }
    }
    else if(commentFLAG == 1)
    {
      //printf("comment used do nothing\n");
      //fflush(stdout);
    }
    /*****************************************************************************************
    This is the background command area of the smallShell
    How this works is that my eachcommand array of strings has the & added as the last element
    however that shouldn't be in the array when I run the execvp command so I first take that index
    and change it to null.
    then from there I can begin the fork process and when in the child when I call execvp this time
    I pass another argument to the function. the WNOHANG that was mentioned in the program specs
    tells us to returns immediately instead of waiting with waitpid this by doing this the shells
    command prompt will still be able to be used.
    Also if the redirection flag is set then we have to run the redirection for that as well.
    The way I do the redirection for the background is the same way specified in the lecture 3.4
    plus from this source http://www.cs.loyola.edu/~jglenn/702/S2005/Examples/dup2.html
    ******************************************************************************************/
    else if(backgroundFLAG == 1)
    {
      if (redirectionFLAG == 1)
      {
        childExitMethod = -5;
        pid_t rdSpawnPID;
        //have the parent fork off the child and then I do the input/output redirection like it says in the program specs
        rdSpawnPID = fork();
        if (rdSpawnPID == -1)
        {
          perror("Hull breach\n");
          exit(1);
        }
        else if (rdSpawnPID == 0)
        {
          int in, out, result;
          //also need to get flag value which tells me if file was specified or not
          //this flag will be used to determine if stdin/stdout should be sent to dev/null
          int inFileValid = 0;
          int outFileValid = 0;
          if (lessThanFLAG == 1)
          {
            //if this is set I have to get the file name from my eachCommand array.
            int y;
            char * lessthanFile;
            for (y = 0; y < commandCounter; y++)
            {
              if (strcmp("<", eachCommand[y]) == 0)
              {
                if((eachCommand[y + 1] != NULL) && (strcmp("&", eachCommand[y + 1]) != 0) )
                {
                  inFileValid = 1;
                  lessthanFile = eachCommand[y + 1];
                }
                else
                {
                  inFileValid = 0;
                }

              }
            }
            //now if the file entered is valid the flag will be 1. In that case open file with that file name
            if (inFileValid == 1)
            {
              //now that I got the file name from the command in shell I can open the file the way it is done in lecture 3.4
              in = open(lessthanFile, O_RDONLY);
            }
            else
            {
              in = open("/dev/null", O_RDONLY);
            }
            //the program specs state that if file connot be open for reading the print error message and exit with 1
            //thats what this if statement will check for
            if (in == -1)
            {
              //https://stackoverflow.com/questions/24717686/how-to-print-a-variable-with-perror
              fprintf(stderr, "cannot open %s for input\n", lessthanFile);
              exit(1);
            }
            //here I replace stdin with the file using dup2
            result = dup2(in, 0);
            //now close the file descriptor
            close(in);
          }
          //now the standord output redirection
          //note I needed if statement so sometimes if both redirections are in the command list
          //this way
          if (greaterThanFLAG == 1)
          {
            //printf("changing stdout\n");
            int z;
            char * greaterthanFile;
            for (z = 0; z < commandCounter; z++)
            {
              if (strcmp(">", eachCommand[z]) == 0)
              {
                if((eachCommand[z + 1] != NULL) && (strcmp("&", eachCommand[z + 1]) != 0) )
                {
                  outFileValid = 1;
                  greaterthanFile = eachCommand[z + 1];
                }
                else
                {
                  outFileValid = 0;
                }
              }
            }
            if (outFileValid == 1)
            {
              //just like in the lecture open the stdout file
              out = open(greaterthanFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }
            else
            {
              out = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }

            if (out == -1)
            {
              fprintf(stderr, "cannot open %s for output\n", greaterthanFile);
              exit(1);
            }
            //change the stdout to the output file specified
            dup2(out, 1);
            close(out);
          }
          //call the execlp function which takes the command the command is still going to be in first element of the array
          execlp(eachCommand[0], eachCommand[0], NULL);
        }
        else
        {
          printf("background pid is %d\n", rdSpawnPID);
          fflush(stdout);
          waitpid(rdSpawnPID, &childExitMethod, WNOHANG);
          backgroundCheckID[backgroundIndexer] = rdSpawnPID;
          backgroundIndexer = backgroundIndexer + 1;
        }
      }
      else
      {
        //printf("now running background area\n");
        //printf("the lessthanflag: %d\ngreaterthanglag: %d\n", lessThanFLAG, greaterThanFLAG);
        commandCounter = commandCounter - 1;
        eachCommand[commandCounter] = NULL;
        childExitMethod = -5;
        /*int x;
        for (x = 0; x < commandCounter; x++)
        {
          printf("%s\n", eachCommand[x]);
        }*/
        pid_t bgSpawnPID;

        bgSpawnPID = fork();
        if(bgSpawnPID == -1)
        {
          perror("Hull breach\n");
          exit(1);
        }
        else if(bgSpawnPID == 0)
        {
          execvp(eachCommand[0], eachCommand);
        }
        else
        {
          printf("background pid is %d\n", bgSpawnPID);
          fflush(stdout);
          waitpid(bgSpawnPID, &childExitMethod, WNOHANG);
          backgroundCheckID[backgroundIndexer] = bgSpawnPID;
          backgroundIndexer = backgroundIndexer + 1;
        }
      }
    }
    /****************************************************************************************************
    This is my redirection area of the shell. Here I will use dup2 to take stdinput and replace it with the file
    that was specified by the user. In the program specs it says that we cant use execvp because with that the
    problem is that the array sent to it will have the < > in it which will not work. So for the p exec function my
    only other option is to use the execlp. Also if the > is used then I will redirect the stdout to the output file .
    Easy to implement since we use the dup2 function which will let use specify the two arguments. Then I close the file descriptor
    Once closed call the execlp function. My redirection is similar to how it was done is lecture 3.4 around 15 minute mark.
    this source also helped me http://www.cs.loyola.edu/~jglenn/702/S2005/Examples/dup2.html
    ****************************************************************************************************/
    else if(redirectionFLAG == 1)
    {
      childExitMethod = -5;
      pid_t rdSpawnPID;
      //have the parent fork off the child and then I do the input/output redirection like it says in the program specs
      rdSpawnPID = fork();
      if (rdSpawnPID == -1)
      {
        perror("Hull breach\n");
        exit(1);
      }
      else if (rdSpawnPID == 0)
      {
        int in, out, result;
        if (lessThanFLAG == 1)
        {
          //if this is set I have to get the file name from my eachCommand array.
          int y;
          char * lessthanFile;
          for (y = 0; y < commandCounter; y++)
          {
            if (strcmp("<", eachCommand[y]) == 0)
            {
              lessthanFile = eachCommand[y + 1];
            }
          }
          //now that I got the file name from the command in shell I can open the file the way it is done in lecture 3.4
          in = open(lessthanFile, O_RDONLY);
          //the program specs state that if file connot be open for reading the print error message and exit with 1
          //thats what this if statement will check for
          if (in == -1)
          {
            //https://stackoverflow.com/questions/24717686/how-to-print-a-variable-with-perror
            fprintf(stderr, "cannot open %s for input\n", lessthanFile);
            exit(1);
          }
          //here I replace stdin with the file using dup2
          result = dup2(in, 0);
          //now close the file descriptor
          close(in);
        }
        //now the standord output redirection
        //note I needed if statement so sometimes if both redirections are in the command list
        //this way
        if (greaterThanFLAG == 1)
        {
          //printf("changing stdout\n");
          int z;
          char * greaterthanFile;
          for (z = 0; z < commandCounter; z++)
          {
            if (strcmp(">", eachCommand[z]) == 0)
            {
              greaterthanFile = eachCommand[z + 1];
            }
          }
          //just like in the lecture open the stdout file
          out = open(greaterthanFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
          if (out == -1)
          {
            fprintf(stderr, "cannot open %s for output\n", greaterthanFile);
            exit(1);
          }
          //change the stdout to the output file specified
          dup2(out, 1);
          close(out);
        }
        //call the execlp function which takes the command the command is still going to be in first element of the array
        execlp(eachCommand[0], eachCommand[0], NULL);
      }
      else
      {
        //printf("the lessthanflag: %d\ngreaterthanglag: %d\n", lessThanFLAG, greaterThanFLAG);
        //fflush(stdout);
        waitpid(rdSpawnPID, &childExitMethod, 0);
      }
    }
    /*************************************************************************************************
    This else statement is for forking a normal process.
    Since the fork is in a while loop it might seem risky. It would be if I didn't
    have the execvp function called in the child process. What that does is kill the program
    and start up that new program so in that case the while loop is not there anymore. Thats
    why it is okay to have the fork inside this while loop. However if the exec function wasn't called
    I would have forked a child and then that child would be in while loop and then it would also
    fork again causing runaway process I think.
    ************************************************************************************************/
    else
    {
      /*******************************************************************************************************
      Now the problem was the SIGINT signal should also kill the child foreground process. However
      with the shell setup the sigint is being ignore. So to combat this and have the child SIGint kill the process
      I need to setup a new sigaction for the sigint signal. Whenver a foregorund process is run. I will setup the
      SIGINT with handler and the intialization like it was shown in the lecture 3.3 video.
      *********************************************************************************************************/
      struct sigaction SIGINT_action = {0};
      SIGINT_action.sa_handler = catchSIGINT;
      sigfillset(&SIGINT_action.sa_mask);
      SIGINT_action.sa_flags = 0;
      sigaction(SIGINT, &SIGINT_action, NULL);
      //Now I need to add an extra null to the end of the array because execvp will require this when I pass
      //the array to that function it will look for the null at the end.
      commandCounter = commandCounter + 1;
      eachCommand[commandCounter] = NULL;
      childExitMethod = -5;
      pid_t spawnPID;

      spawnPID = fork();
      if (spawnPID == -1)
      {
        perror("Hull breach!\n");
        exit(1);
      }
      else if(spawnPID == 0)
      {
        //using the execvp is better since lecture said p gives path variable
        //printf("in child process wooo\n");
        //fflush(stdout);
        //in lecture 3.1 46 minute mark using execvp in that same way.
        if (execvp(eachCommand[0], eachCommand) < 0)
        {
          fprintf(stderr, "%s: ", eachCommand[0]);
          perror("");
          exit(1);
        }
      }
      else
      {
        waitpid(spawnPID, &childExitMethod, 0);
      }
    }

  }

  return 0;
}

