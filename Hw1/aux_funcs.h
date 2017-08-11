#include "parser.h"
#include <wait.h>
#include <unistd.h>
#include <fcntl.h>
int eval_input(input *inp);
char* get_command(command *command);
char* get_parsed_command_name(pc* command);
void execute(input *,int);
int pre_parse(input *);
void execute_subshell(input *,int);
