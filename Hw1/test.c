//#include "parser.h"
#include "aux_funcs.h"

int main() {
    char buffer[MAX_COMMAND_LINE_LENGTH+1];
    char temp;
    int index = 0;
    /* My variables */
    char *single_command_args[3];
    char *subshell_command_args[2]; 
    char command[MAX_COMMAND_LINE_LENGTH];
    pid_t pid,wpid,pid2;
    int child_status;
    int result,sub_shell_result;
    int quit_flag = 0,bg_flag = 0;
    int j;
    int inp_fd,out_fd;
    int fd[2];
    int i;
    int pipe_read,saved_stdout;
    input* subshell_command;
    printf("at the beginning\n");
    /*--------------*/
    printf("> ");
    fflush(stdout);
    while(1) {
	waitpid(-1,&child_status,WNOHANG);
	if(bg_flag)
	{
		printf("> ");
		fflush(stdout);
		bg_flag = 0;
	}
	if((read(fd[0],command,MAX_COMMAND_LINE_LENGTH))>=0)
		printf("%s\n",command);
        if ( (temp = fgetc(stdin)) == EOF )
	{printf("end of file contracted\n");
            break;}
        else if ( temp == '\n') {
	printf("newline entered\n");
            buffer[index++] = temp;
        buffer[index] = '\0';
        index = 0;
        input* inp = parse(buffer);
        print_input(inp);
            result = eval_input(inp);
            switch(result) {
                
            case QUIT:
                quit_flag = 1;
                break;
	    case LBP:
		break;
	    case SUB_SHELL:
		pipe_read = 0;
		pipe(fd);
		char* token;
		const char s[2]="\n";
		//dup2(fd[1],1);
		token = strtok(inp->commands->info.subshell, s);
		write(fd[1],strcat(token," ; quit\n"),strlen(token) + 8); //making sure subshell will quit
		close(fd[1]);
		read(fd[0],command,MAX_COMMAND_LINE_LENGTH);
		close(fd[0]);	
				
		printf("command is : %s",command);
		pid = fork();
		if(pid == 0)
		{
			dup2(fd[0],0); //read from pipe. stdin is directed to pipe.
			//close(fd[0]);
			close(fd[1]);
			subshell_command_args[0] = "./test";
			subshell_command_args[1] = NULL;
			execvp(subshell_command_args[0],subshell_command_args);
				
		}//end if
		wpid = waitpid(pid,&child_status,0);
		if(WEXITSTATUS(child_status) == 2)
			quit_flag = 1;	
		break;
            case SINGLE_COMMAND:
                    	pid = fork();
                    	if(pid == 0) { /*Child*/
				
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
                            if( dup2(inp_fd,0) >= 0) //INPUT REDIRECTION : stdin is directed to inp_fd
                            {    
				if((out_fd = open(inp->commands->output, O_WRONLY|O_CREAT, (mode_t)0777)) < 0){ //then check output, create if not exists
					printf("%s not found\n",inp->commands->output);
					exit(1);
				}
				else //redirect output below
				{
					if(dup2(out_fd,1)>0) //OUTPUT REDIRECTION : stdout is directed to out_fd
					{
						
						execute(inp,0);
					}
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
	    {
              	   break;}		
            if(pid!=0)
            {
               	clear_input(inp);
               	printf("> ");
               	fflush(stdout);
            }
        }
        else
        {    buffer[index++] = temp; printf("%c\n",temp);}
    }
    return 0;
}
