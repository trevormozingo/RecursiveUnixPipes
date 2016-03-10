 
/*
Trevor Mozingo
CPT_S 360 - LAB 3
SH - simulator
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <envz.h> /* use envz.h for searching environment variables - http://man7.org/linux/man-pages/man3/envz.3.html */
			
char line[128], buff[128], buff1[128], buff2[128], util[128], util2[128];	/* utility buffers for string proccesing */

char *paths[] = {"/usr/local/sbin/","/usr/local/bin/","/usr/sbin/","/usr/bin/","/sbin/","/bin/","/usr/games/","/usr/local/games/","."};									/* command directories -- these are where the unix commands are located -> needed for the execve command */


int prompt()						/* prompt to get user input */
{
	printf("trevormozingo@LABSHELL $ "); 
	fgets(line, sizeof(line), stdin); 		/* store in the line buffer */
}

void refresh(char *s[]) {				/* refresh clears the auxilary memory that was used for the arguments */
	int i;	
	for (i = 0; i < 16; i++)
	{
		free(s[i]);
		s[i] = NULL;
	}
}

void tok(char *s[], char *s2)				
{
							/* given any command, this function will put each element into an argument 								array. the elements from *s2 string, will be broken up and stored into the 								*s[], which serves as the argument array for running commands in execve
							E.G -->
								*s2 = "gcc -m32 t.c"
								*s[] = {"gcc","m2","t.c"}			 */
	int i;
	char *s3;

	strcpy(buff, s2);				/* store in temporary array for saftey of strings */

	for (i = 0; i < 16; i++)
	{
		s[i] = (char *)malloc(128);		/* first allocate aux array for storing argument */
	}

	s3 = strtok(buff, " \n");			/* tokenize each component and store into the array */
	i = 0;

	while (s3)					/* while not the end of the command string, store components into array */
	{
		strcpy(s[i], s3);			
		s3 = strtok(NULL, " \n");
		i++;
	}

	while (i < 16) {				/* free up any unused auxillary space */
		free(s[i]);
		s[i] = NULL;
		i++;
	}

}

void execute2(char *args[], char *env[])
{
	int i;
	
	for (i = 0; args[i]; i++) 
	{
		if ((strcmp(args[i], "<") == 0) || (strcmp(args[i], ">") == 0) ||(strcmp(args[i], ">>") == 0))
		{
			/* depending on the redirection operator inside the command ---> */

			if (strcmp(args[i], "<") == 0) {		/* set input redirection to readonly from file */
				close(0);
				strcpy(util, args[i+1]);
				open(util, O_RDONLY);			/*read only */
			}
		
			else if (strcmp(args[i], ">") == 0) 		/* set output redirection to add to top of file or create a 										file and add to it */
			{
			 	close(1);
				strcpy(util, args[i+1]);
				open(util, O_WRONLY|O_CREAT,0664);	/* Write only */
			}

			else if (strcmp(args[i], ">>") == 0)		/* set output redirection to append to a file */
			{ 
				close(1);
				strcpy(util, args[i+1]);
				open(util, O_RDWR|O_APPEND,0664);	/* read and write */
			}

			free(args[i]);			/* you need to free the '>' '<' or '>>' from the args array because it is 								not recognized by commands like cat	
							E.G.
							if args[] = {"cat", "testFile", ">","outfile" }		
							then calling execve will think you are calling cat on files 								"testFile",">",and "outfile"
							so... free '>' from the array so that it executes only "cat testFile"
							from args [] = {"cat","testFile", NULL .... } 
							from the above code, we already set the redirection to outfile	

																*/
			args[i] = NULL;

		}

	}

	strcpy(util, args[0]);
	for (i = 0; i < 9; i++)
	{
								/* this code will search all of the paths for the unix 									commands. so if command is cd --> it will loop through and 									append cmd to the appropriate directories and test each one 									until the command is executed
							
								--> /bin/cmd ----> fails
								    /bin/local/cmd --> fails
								    /bin/games/cmd --> passes -> executes -> exits */
		strcpy(util2, paths[i]);
		strcat(util2, args[0]);
		strcpy(args[0], util2);

		if (execve(args[0], args, env) != -1)  		/* if the execution is successful, 
								then it will NOT return here */
		{					
			return;					/* will never reach here if code works */
		}
		
		strcpy(args[0], util);				/* get the original command unappended to test directory */	
		if (execve(args[0], args, env) != -1)
		{
			return;
		}
	}

	printf("error\n");
	return;
}

void execute(char *command, char *env[]) 
{
	int i = 0, pid, status, pipefiles[2];			/* pipefiles will hold the file descriptors after pipe(pipefiles) */
	char *head[16], *tail = NULL;

	if (command == NULL) { return; }			/* if nothing in command then return (e.g if you enter command like  									ls |) --> then the tail will be empty, so when calling tail, it 									should return immediately */

	if (command[0] == '\0') { return; }			/* if nothing was entered by user */
	
	/* split command to head and tail */

	strcpy(buff, command);
	strtok(buff, "|");
	strcpy(buff2, buff);
	tail = strtok(NULL, "");
	tok(head, buff2);	
			
	/* the above commands split the command to head and tail depending on if it is a pipe command 
	
	so if command --> "cat someFile | grep word | wc > outfile" 
	then:
	head = "cat someFile"
	tail = "grep word | wc > outfile" 
	you would pass tail recursively into 'this' function (execute) and it would bes split 			
	into its head and tail, etc.

	head will be executed as parent process, and tail will be recursively be executed in the child process
	*/
	
	
	if (head[0] == NULL) { refresh(head); return; }	/* if you reached the end of the command 
							(all were already executed), then return */
	
	if (tail == NULL)		/* if the tail is null (aka. no pipe in command), then execute command straight up */
	{
		execute2(head,env);
		refresh(head);
		return;
	}

	pipe(pipefiles);		/* pipe creates channel for reading and writing
					  
					   write is pipefiles[1]
					   read is pipefiles[0]
				
					   so after parent executes -> its output flows through the channel to the child pipe	
					   parent --> pipefiles[1]=====this is a pipe========pipefiles[0] ---> child
					   (so if command contains a pipe -> redirect output data through the pipe)
									
					"""PIPES DIRECT TO THE FLOW OF STDIN & STDOUT. PROGRAM INPUT AND OUTPUT 		
					USES STDIN & STDOUT, SO BY OPENING A PIPE, THE FLOW OF PROCESS INPUT AND OUTPUT
					WILL OCCUR THROUGH THAT PIPE (you DONT need write or read for program input & output) """
					
					READ & WRITE IS just a way of explicitly writing to a STDIN & STDOUT
				
	
					/* more pipe information can be learned here:
					https://www.cs.princeton.edu/courses/archive/fall04/cos217/lectures/21pipes.pdfe 

					http://cse.iitkgp.ac.in/~pallab/lec2.pdf
					note: not a citation, just a link that helps my understanding
					*/

	pid = fork();			 /* fork a new process from this point */
	
	if (pid)			 /* note: this point will only be reached if it is a pipe command 
					    because this is a pipe command, redirect data flow through the pipe
														*/
	{


	/*
	the parent is the writer so needs to open the write end of the pipe, and direct output data flow through.
	the following shows the fd table, and one happens after each command
	
	step 1			step 2			step 3			step 4 		 	step 5
	

read		write	 |			  |read		write   |		   | read		write	
			 |	                  |                     |		   | 	
--->	0 stdin		 | 			  | -->	0 stdin		| 		   |  --> 0 stdin
	1 stdout  -->	 |   close(1)		  |	1 (null)  -->	| dup(pipefiles[1])|      1 pipefiles[1] -->
	2 strerr  -->	 |   close(pipefiles[0])  |	2 strerr  -->	| 		   |      2 strerr	 -->
 	3 pipefiles[0]	 | 			  | 	3 (null)        |		   |	  
	4 pipefiles[1] 	 | 	 		  |     pipefiles[1]	| 		   | 

		So now the system will always write the program output to the pipe through the write end "pipefiles[1]"
		
	*/


		close(pipefiles[0]); 	 	
		close(1);		
		dup(pipefiles[1]);	
		execute2(head, env);
		pid = wait(&status);
		//printf("%d", status);
	}

	else 
	{

/*
	the child is the reader so needs to create open read end of the pipe
	the following shows the fd table, and one happens after each command
	
	step 1			step 2			step 3			step 4 		 	step 5
	

read		write	 |			  |read		write   |		   | read		write	
			 |	                  |                     |		   | 	
--->	0 stdin		 | 			  | -->	0 (null)	| 		   |  --> 0 pipefiles[0]
	1 stdout  -->	 |   close(0)		  |	1 stdout  -->	| dup(pipefiles[0])|      1 stdout	 -->
	2 strerr  -->	 |   close(pipefiles[1])  |	2 strerr  -->	| 		   |      2 strerr	 -->
 	3 pipefiles[0]	 | 			  | 	3 pipefiles[0]  |		   |	  
	4 pipefiles[1] 	 | 	 		  |     4 (null)	| 		   | 

		So now the system will always read input through the pipe read end of the channel pipefiles[0]"
		
*/

		close(pipefiles[1]);	
		close(0);		
		dup(pipefiles[0]);	

		execute(tail, env);	
		refresh(head);	
		exit(0);
	}
	
	refresh(head);
	return;
}

int main(int argc, char *argv[], char *env[]) 
{
	int pip, status, i = 0, len = 0;
	char *head[16];
	char*dir;
redo:
	while (1)
	{
		prompt();
		strcpy(buff, line);
		tok(head, buff);

		if (head[0] == NULL) { refresh(head); goto redo; }
		
		if (strcmp(head[0], "cd") == 0) 			/* if cd --> home if no path, otherwise go to path*/
		{ 
			if (head[1] == NULL)
			{	
				for (i = 0; env[i]; i++)		/* use envz.h for searching environment variable
									 http://man7.org/linux/man-pages/man3/envz.3.html */
				{
					len += strlen(env[i]) + 1; 	/* needs to be position in env */
				}

				dir = envz_entry(*env,len,"HOME");
				chdir(dir+5);
			}	
	
			else
			{
				chdir(head[1]);
			}

			refresh(head); goto redo; 
		}

		if (strcmp(head[0], "exit") == 0) { refresh(head); exit(1); } /* if exit --> exit(1) */
	
		pip = fork();
		
		if (pip)
		{
			pip = wait(&status);
			printf("child exit process: %d\n", status);
		}
		
		else 
		{
			execute(line, env); /* wait for child process to finish */
			exit(0);
		}

		refresh(head);

	}

}










































































