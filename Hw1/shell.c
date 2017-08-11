
#include "aux_funcs.h"
#include "linkedlist.h"
int main() {
    char buffer[MAX_COMMAND_LINE_LENGTH+1];
    char temp;
    int index = 0;
    /* My variables */
    char *single_command_args[3];
    char *subshell_command_args[2]; 
    char command[MAX_COMMAND_LINE_LENGTH];
    pid_t pid0,pid=1,wpid,pid2=0,pid3;
    int child_status;
    int result,subshell_result;
    int quit_flag = 0,bg_flag = 0,pipe2_flag=0;
    int j;
    int inp_fd,out_fd;
    int fd[2],fd2[2];
    int i;
    int pipe_read,pipe2_read;
    input* subshell_command;
    /*linked list for background processes */
    struct node *n;
    head=NULL;
    /*--------------*/
    printf("> ");
    fflush(stdout);
    while(1) {
	wpid = waitpid(-1,&child_status,WNOHANG);
	if(wpid>0)
		remove_process(wpid+1);
	if(bg_flag)
	{
		printf("> ");
		fflush(stdout);
		bg_flag = 0;
	}
    if ( (temp = fgetc(stdin)) == EOF )
        break;
    else if ( temp == '\n') {
        buffer[index++] = temp;
    buffer[index] = '\0';
    index = 0;
    input* inp = parse(buffer);
    result = eval_input(inp);
    switch(result) {
                
       	case QUIT:
            quit_flag = 1;
            wait(NULL);
            break;
	    case LBP:
	    	list_bg_processes(n);
			break;
		case BACKGROUND_SUB_SHELL:
			pid0 = fork();
			if(pid0)
			{
				bg_flag = 1;
				insert_process(pid0+1);
				continue;
			}
			else if(pid0==0)
			{
				setpgid(0,0);
				
				pid2 = fork();
				
				if(pid2 == 0)
				{
						
					execute_subshell(inp,0);
					exit(1);
				}
				wpid = waitpid(pid2,&child_status,0);
				printf("[%d] retval:%d \n",wpid,WEXITSTATUS(child_status));
				fflush(stdout);
				exit(1);
			}

			break;
	    case SUB_SHELL:
	    pid0 = fork();
	    if(pid0==0)
		{
	    	if(inp->del=='|')
			{
				pipe2_read = 0;
				pipe2_flag = 1;
			}
			for(i=0;i<inp->num_of_commands;i++)
			{
				if(pipe2_flag)
					pipe(fd2);
				if((inp->commands+i)->type == SUBSHELL) // there is a subshell in commands
				{
					pid = fork(); //open subshell
					if(pid == 0)
					{
						if((inp->commands+i)->input)
						{
							if((inp_fd = open(inp->commands->input, O_RDONLY)) < 0){
                            	printf("%s not found\n",inp->commands->input);
                            	exit(1);
                        	}
                        	else
                            {	dup2(inp_fd,0);close(inp_fd);} //INPUT REDIRECTION : stdin is directed to inp_fd
                		}
                		if((inp->commands+i)->output)
						{
							if((out_fd = open((inp->commands+i)->output, O_WRONLY|O_CREAT, (mode_t)0777)) < 0){
								printf("%s not found\n",inp->commands->output);
								exit(1);
							}
							else
							{	dup2(out_fd,1);close(out_fd);}
						}
						if( pipe2_flag && pipe2_read != 0) //if pipe_read is other than 0(stdin), then redirect stdin to pipe read. i.e make the child process read from pipe. Then close unnecessary pipe_read.
						{
							dup2(pipe2_read,0);
							close(pipe2_read);
						}
						if(pipe2_flag && fd2[1]!=1 && i+1!=inp->num_of_commands) // if write of pipe is not stdout, then redirect stdout to write of pipe. i.e. make the child process write to pipe.		
						{
							dup2(fd2[1],1);
							close(fd2[1]);
						}
						execute_subshell(inp,i);
						exit(1);
					}//end pid == 0 if
					waitpid(pid,&child_status,0);//wait for subshell to exit.
					if(pipe2_flag)
					{
						close(fd2[1]);
						pipe2_read = fd2[0];
					}
				}//end first subshell if
				else
				{
					pid = fork(); //open subshell
					if(pid == 0)
					{
						if((inp->commands+i)->input)
						{
							if((inp_fd = open(inp->commands->input, O_RDONLY)) < 0){
                            	printf("%s not found\n",inp->commands->input);
                            	exit(1);
                        	}
                        	else
                            {	dup2(inp_fd,0);close(inp_fd);} //INPUT REDIRECTION : stdin is directed to inp_fd
                		}
                		if((inp->commands+i)->output)
						{
							if((out_fd = open((inp->commands+i)->output, O_WRONLY|O_CREAT, (mode_t)0777)) < 0){
								printf("%s not found\n",inp->commands->output);
								exit(1);
							}
							else
							{	dup2(out_fd,1);close(out_fd);}
						}
						if( pipe2_flag && pipe2_read != 0) //if pipe_read is other than 0(stdin), then redirect stdin to pipe read. i.e make the child process read from pipe. Then close unnecessary pipe_read.
						{
							dup2(pipe2_read,0);
							close(pipe2_read);
						}
						if(pipe2_flag && fd2[1]!=1 && i+1!=inp->num_of_commands) // if write of pipe is not stdout, then redirect stdout to write of pipe. i.e. make the child process write to pipe.		
						{
							dup2(fd2[1],1);
							close(fd2[1]);
						}
						execute(inp,i);
						exit(1);
					}//end pid == 0 if
					waitpid(pid,&child_status,0);//wait for subshell to exit.
					if(pipe2_flag)
					{
						close(fd2[1]);
						pipe2_read = fd2[0];
					}
				}//end normal command
			}//end for	
			close(pipe2_read);
			exit(1);
		}
		waitpid(pid0,&child_status,0);
		pipe2_flag = 0;
		break;
        case SINGLE_COMMAND:
            pid = fork();
            if(pid == 0) {
				execute(inp,0);
			}
			waitpid(pid,&child_status,0);
            break;
	    case IO_REDIRECTION:
		 	pid = fork();
            if(pid == 0) {
              	if((inp_fd = open(inp->commands->input, O_RDONLY)) < 0){//first check input
                    printf("%s not found\n",inp->commands->input);
                    exit(1);
                }
                else
                {//if exists then redirect it below
                    if( dup2(inp_fd,0) >= 0){ //INPUT REDIRECTION : stdin is directed to inp_fd
						if((out_fd = open(inp->commands->output, O_WRONLY|O_CREAT, (mode_t)0777)) < 0){ //then check output, create if not exists
							printf("%s not found\n",inp->commands->output);
							exit(1);
						}
						else{ //redirect output below	
							if(dup2(out_fd,1)>0) //OUTPUT REDIRECTION : stdout is directed to out_fd
								execute(inp,0);
						}     
                    }
                    else
                        printf("could not dup. inp_fd = %d, stdin = %d\n",inp_fd,STDIN_FILENO);
                 }
            }
		    waitpid(pid,&child_status,0);
            break;
        case INPUT_REDIRECTION:
                    pid = fork();
                    if(pid == 0) {
                        if((inp_fd = open(inp->commands->input, O_RDONLY)) < 0){
                            printf("%s not found\n",inp->commands->input);
                            exit(1);
                        }
                        else
                        {
                            if( dup2(inp_fd,0) >= 0) //INPUT REDIRECTION : stdin is directed to inp_fd
                            {    
				execute(inp,0);     
                            }
                            else
                            	printf("could not dup. inp_fd = %d, stdin = %d\n",inp_fd,STDIN_FILENO);
                            }
                    }
		    waitpid(pid,&child_status,0);
                    break;
		case OUTPUT_REDIRECTION:
			pid = fork();
                    	if(pid == 0) {
				if((out_fd = open(inp->commands->output, O_WRONLY|O_CREAT, (mode_t)0777)) < 0){
					printf("%s not found\n",inp->commands->output);
					exit(1);
				}
				else
				{
					if(dup2(out_fd,1)>0) //OUTPUT REDIRECTION : stdout is directed to out_fd
					{
						//close(out_fd);
						execute(inp,0);
					}
					else
                        printf("could not dup. inp_fd = %d, stdin = %d\n",out_fd,STDOUT_FILENO);
				}
			}
			waitpid(pid,&child_status,0);
			break;
		case BACKGROUND_PROCESS:
			pid = fork();
			if(pid){
				bg_flag = 1;
				insert_process(pid+1);				
				continue;	
			}
			else if(pid==0)
			{
				setpgid(0,0);
				
				pid2 = fork();
				
				if(pid2 == 0)
				{
						
					execute(inp,0);
					exit(1);
				}
				
				wpid = waitpid(pid2,&child_status,0);
				
				printf("[%d] retval:%d \n",wpid,WEXITSTATUS(child_status));
				fflush(stdout);
				exit(1);
			}
			break;
		case SEQUENTIAL_COMMAND_PIPE:
			pid = fork();
			if(pid == 0)
			{
				if(inp->commands->input)
				{
					if((inp_fd = open(inp->commands->input, O_RDONLY)) < 0){
                            			printf("%s not found\n",inp->commands->input);
                            			exit(1);
                        		}
                        		else
                            	{	dup2(inp_fd,0);close(inp_fd);} //INPUT REDIRECTION : stdin is directed to inp_fd
                }
				
				pipe_read = 0; //whether directed or not, will take the input from stdin.
				for(i=0;i < inp->num_of_commands - 1;i++)
				{
					pipe(fd);
					pid2 = fork();
					if(pid2==0)	
					{
						if(pipe_read != 0) //if pipe_read is other than 0(stdin), then redirect stdin to pipe read. i.e make the child process read from pipe. Then close unnecessary pipe_read.
						{
							dup2(pipe_read,0);
							close(pipe_read);
						}
						if(fd[1]!=1) // if write of pipe is not stdout, then redirect stdout to write of pipe. i.e. make the child process write to pipe.		
						{
							dup2(fd[1],1);
							close(fd[1]);
						}
						execute(inp,i);
					}
					close(fd[1]);
					pipe_read = fd[0];
				}//end for
				dup2(pipe_read,0);
				close(pipe_read);
				if((inp->commands+i)->output)
				{
					if((out_fd = open((inp->commands+i)->output, O_WRONLY|O_CREAT, (mode_t)0777)) < 0){
						printf("%s not found\n",inp->commands->output);
						exit(1);
					}
					else
					{	dup2(out_fd,1);close(out_fd);}
				}
				execute(inp,i);
			}
			waitpid(pid,&child_status,0);
			break;
		case SEQUENTIAL_COMMAND_SMCLN:
			for(i=0;i < inp->num_of_commands;i++)
			{
				pid = fork();
				if(pid==0)
				{
					if((inp->commands+i)->input)
					{
						if((inp_fd = open((inp->commands+i)->input, O_RDONLY)) < 0){
                            				printf("input : %s not found\n",inp->commands->input);
                            				exit(1);
                        			}
                        			else
                            			{	dup2(inp_fd,0);close(inp_fd);} //INPUT REDIRECTION : stdin is directed to inp_fd
					}//end input if
					if((inp->commands+i)->output)
					{
						if((out_fd = open((inp->commands+i)->output, O_WRONLY|O_CREAT, (mode_t)0777)) < 0){
							printf("output : %s not found\n",inp->commands->output);
							exit(1);
						}
						else
						{	dup2(out_fd,1);close(out_fd);}
					}//end output if
				
					execute(inp,i);
                            	}
				waitpid(pid,&child_status,0);			
			}
			break;
                default:
                    break;
                }
           
            if(quit_flag)
              	break;		
            if(pid!=0)
            {
               	clear_input(inp);
               	printf("> ");
               	fflush(stdout);
            }
        }
        else
            buffer[index++] = temp;
    }
    return 0;
}
