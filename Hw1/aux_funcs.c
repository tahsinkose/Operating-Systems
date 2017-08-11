#include "aux_funcs.h"

char* get_parsed_command_name(pc* comm){
	if(!comm)
		return "null";
	return comm->name;
}
char* get_command(command *command){
	if(!command)
		return "null";
	if ( command->type == NORMAL ) {
        return get_parsed_command_name(command->info.com);
    }
    else {
        return command->info.subshell;
    }
}
int eval_input(input *inp){
	char quit[5]="quit";
	char list_background_processes[4]="lbp";
	if( !inp )
		return 1;

	if(pre_parse(inp) && inp->background)
		return BACKGROUND_SUB_SHELL;
	else if(pre_parse(inp))
		return SUB_SHELL;
	else if(strcmp(get_command(inp->commands),quit)==0)
		return QUIT;
	else if(strcmp(get_command(inp->commands),list_background_processes)==0)
		return LBP;
	else if(inp->commands->input && inp->commands->output && !(inp->del))
		return IO_REDIRECTION;
	else if(inp->commands->input && !(inp->del))
		return INPUT_REDIRECTION;
	else if(inp->commands->output && !(inp->del))
		return OUTPUT_REDIRECTION;
	else if(inp->background)
		return BACKGROUND_PROCESS;
	else if(inp->del=='|')
		return SEQUENTIAL_COMMAND_PIPE;
	else if(inp->del==';')
		return SEQUENTIAL_COMMAND_SMCLN;
	else if(inp->num_of_commands == 1)
		return SINGLE_COMMAND;
	
}

void execute(input* inp,int index)
{
			char *single_command_args[3];
			int j;
			single_command_args[0] = get_command(inp->commands+index);
            pc* parsed_single_command = (inp->commands+index)->info.com;
            for(j=0;j < parsed_single_command->num_of_args;j++)
                single_command_args[j+1] = parsed_single_command->arguments[j];
            single_command_args[j+1] = NULL;
			
            execvp(single_command_args[0],single_command_args);
			
}

int pre_parse(input* inp){
	for(int i=0;i<inp->num_of_commands;i++)
	{
		if((inp->commands+i)->type == SUBSHELL)
			return 1;
	}
	return 0;
}

void execute_subshell(input* inp,int i){
	input* subshell_command;
	int subshell_result;
	int inp_fd,out_fd;
	pid_t pid,wpid,pid2,pid3;
	int fd[2];
	int child_status;
	int j;
	int pipe_read;
	subshell_command = parse((inp->commands+i)->info.subshell);
	subshell_result = eval_input(subshell_command);
	/*printf("---------IN SUBSHELL--------------\n");
	print_input(subshell_command);
	printf("---------IN SUBSHELL--------------\n");*/
	switch(subshell_result){
		case QUIT:
			break;
		case SINGLE_COMMAND:
			pid2 = fork();
            if(pid2 == 0) 
				execute(subshell_command,i);
			waitpid(pid2,&child_status,0);
            break;
		case INPUT_REDIRECTION:
            pid2 = fork();
            if(pid2 == 0) {
                if((inp_fd = open((subshell_command->commands+i)->input, O_RDONLY)) < 0){
                    printf("%s not found\n",(subshell_command->commands+i)->input);
                    exit(1);
                }
                else
                {
                    if( dup2(inp_fd,0) >= 0) //INPUT REDIRECTION    
						execute(subshell_command,i);     
                    else
                        printf("could not dup. inp_fd = %d, stdin = %d\n",inp_fd,STDIN_FILENO);
                            
                }
			}//end if
		    waitpid(pid2,&child_status,0);
            break;
		case IO_REDIRECTION:
		 	pid2 = fork();
            if(pid2 == 0) {
                if((inp_fd = open((subshell_command->commands+i)->input, O_RDONLY)) < 0){//first check input
                    printf("%s not found\n",(subshell_command->commands+i)->input);
                    exit(1);
                }
                else
                {//if exists then redirect it below
                    if( dup2(inp_fd,0) >= 0) //INPUT REDIRECTION :
                    {    
						if((out_fd = open((subshell_command->commands+i)->output, O_WRONLY|O_CREAT, (mode_t)0777)) < 0){ //then check output, create if not exists
							printf("%s not found\n",(subshell_command->commands+i)->output);
							exit(1);
						}
						else //redirect output below
						{
							if(dup2(out_fd,1)>0) //OUTPUT REDIRECTION : 
								execute(subshell_command,i);
						}     
                    }
                    else
                        printf("could not dup. inp_fd = %d, stdin = %d\n",inp_fd,STDIN_FILENO);
                 }
            }
		    waitpid(pid2,&child_status,0);
            break;
		case OUTPUT_REDIRECTION:
			pid2 = fork();
            if(pid2 == 0) {
				if((out_fd = open((subshell_command->commands+i)->output, O_WRONLY|O_CREAT, (mode_t)0777)) < 0){
					printf("%s not found\n",(subshell_command->commands+i)->output);
					exit(1);
				}
				else
				{
					if(dup2(out_fd,1)>0) //OUTPUT REDIRECTION 
						execute(subshell_command,i);
					else
                	    printf("could not dup. inp_fd = %d, stdin = %d\n",out_fd,STDOUT_FILENO);
				}
			}
			waitpid(pid2,&child_status,0);
			break;
		case SEQUENTIAL_COMMAND_PIPE:
			pid2 = fork();
			if(pid2 == 0)
			{
				if((subshell_command->commands+i)->input)
				{
					if((inp_fd = open((subshell_command->commands+i)->input, O_RDONLY)) < 0){
                       	printf("%s not found\n",(subshell_command->commands+i)->input);
                        exit(1);
                    }
                    else
                       {	dup2(inp_fd,0);close(inp_fd);} //INPUT REDIRECTION : stdin is directed to inp_fd
                }

				pipe_read = 0; //whether directed or not, will take the input from stdin.
				for(j=0;j < subshell_command->num_of_commands - 1;j++)
				{
					pipe(fd);
					pid3 = fork();
					if(pid3==0)	
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
						execute(subshell_command,j);
					}
					close(fd[1]);
					pipe_read = fd[0];
				}//end for
				dup2(pipe_read,0);
				close(pipe_read);
				if((subshell_command->commands+i)->output)
				{
					if((out_fd = open((subshell_command->commands+i)->output, O_WRONLY|O_CREAT, (mode_t)0777)) < 0){
						printf("%s not found\n",(subshell_command->commands+i)->output);
						exit(1);
					}
					else
					{	dup2(out_fd,1);close(out_fd);}
				}
				execute(subshell_command,j);
			}
			waitpid(pid2,&child_status,0);
			break;
		case SEQUENTIAL_COMMAND_SMCLN:
			for(j=0;j < subshell_command->num_of_commands;j++)
			{
				pid2 = fork();
				if(pid2==0)
				{
					/*if(pipe2_flag)
					{	close(fd2[0]);
					close(fd2[1]);}*/
					if((subshell_command->commands+j)->input)
					{
						if((inp_fd = open((subshell_command->commands+j)->input, O_RDONLY)) < 0){
                            	printf("input : %s not found\n",(subshell_command->commands+j)->input);
                            	exit(1);
                        }
                        else
                        {	dup2(inp_fd,0);close(inp_fd);} //INPUT REDIRECTION : stdin is directed to inp_fd
					}//end input if
					if((subshell_command->commands+j)->output)
					{
						if((out_fd = open((subshell_command->commands+j)->output, O_WRONLY|O_CREAT, (mode_t)0777)) < 0){
							printf("output : %s not found\n",(subshell_command->commands+j)->output);
							exit(1);
						}
						else
						{	dup2(out_fd,1);close(out_fd);}
					}//end output if
					execute(subshell_command,j);
                }
				waitpid(pid2,&child_status,0);			
			}
			break;
	}//end switch
}
