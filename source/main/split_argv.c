/*
 * SPDX-FileCopyrightText: 2016-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define SS_FLAG_ESCAPE 0x8
#ifndef PSRAM_ATTR
#define PSRAM_ATTR __attribute__((section(".ext_ram.data")))
#endif

typedef enum {
	/* parsing the space between arguments */
	SS_SPACE = 0x0,
	/* parsing an argument which isn't quoted */
	SS_ARG = 0x1,
	/* parsing a quoted argument */
	SS_QUOTED_ARG = 0x2,
	/* parsing an escape sequence within unquoted argument */
	SS_ARG_ESCAPED = SS_ARG | SS_FLAG_ESCAPE,
	/* parsing an escape sequence within a quoted argument */
	SS_QUOTED_ARG_ESCAPED = SS_QUOTED_ARG | SS_FLAG_ESCAPE,
} split_state_t;

/* helper macro, called when done with an argument */
#define END_ARG() do { \
    char_out = 0; \
    argv[argc++] = next_arg_start; \
    state = SS_SPACE; \
} while(0)

#define MAX_COMMAND_LEN	32 // old = 16,
#define MAX_ACTOR_LEN	32
#define MAX_Payload_LEN	4096	//2048
void UpperStr(char *strBuff) {
	char s[MAX_Payload_LEN];
	strcpy(s, strBuff);
	int i;
	for (i = 0; s[i] != '\0'; i++) {
		if (s[i] >= 'a' && s[i] <= 'z') {
			s[i] = s[i] - 32;
		}
	}
	strcpy(strBuff, s);
	return;
}
int argcount = 0;
char *token;
PSRAM_ATTR char in_ptr[MAX_Payload_LEN] = {0};
char Actor_str[MAX_ACTOR_LEN] = {0};
char Command_str[MAX_ACTOR_LEN] = {0};
PSRAM_ATTR char Payload[MAX_Payload_LEN] = {0};

size_t esp_console_split_argv(char *line, char **argv, size_t argv_size) {
	size_t argc = 0;
	int end_index = -1;
	int start_index = -1;
	int depth = 0;
	int in_quotes = 0;
	char *cmd_start = NULL;
	memset(in_ptr, 0, sizeof(in_ptr));
	memset(Actor_str, 0, sizeof(Actor_str));
	memset(Command_str, 0, sizeof(Command_str));
	memset(Payload, 0, MAX_Payload_LEN);
	strncpy(in_ptr, line, (sizeof(in_ptr)-1));
	if(strlen(line) < 5)
	{
		return 0;   //invalid command
	}
	cmd_start = strchr(in_ptr, '<');
	if (cmd_start == NULL)
	{
		return 0;
	}

	char *first_open = strchr(cmd_start, '(');
	if (first_open == NULL)
	{
		return 0;   //invalid command
	}

	start_index = (int)(first_open - cmd_start) + 1;
	depth = 1;
	for (int i = start_index; cmd_start[i] != '\0'; i++)
	{
		char ch = cmd_start[i];
		if (ch == '"')
		{
			/* A quote is escaped only when preceded by odd backslashes. */
			int bs_count = 0;
			for (int j = i - 1; j >= start_index && cmd_start[j] == '\\'; --j) {
				bs_count++;
			}
			if ((bs_count % 2) == 0) {
				in_quotes = !in_quotes;
			}
			continue;
		}
		if (in_quotes)
			continue;
		if (ch == '(')
			depth++;
		else if (ch == ')')
		{
			depth--;
			if (depth == 0)
			{
				end_index = i;
				break;
			}
		}
	}
	if ((end_index == -1) || (end_index < start_index))
	{
		return 0;   //invalid command
	}
	if (end_index == start_index)
	{
		Payload[0] = '\0';
	}
	else
	{
		strncpy((char*)Payload, (char*)&cmd_start[start_index], (end_index - start_index));
	}
	
	token = strtok(cmd_start, ".");
	if (token != NULL) {
		argc++;
		UpperStr(token);
		strcpy (Actor_str,token);
		argv[0] =  Actor_str;
		token = strtok(NULL, "(");
		if (token != NULL) {
			argc++;
			UpperStr(token);
			strcpy (Command_str,token);
			argv[1] = Command_str;
			argc++;
			argv[2] = Payload;
		} else
			printf("\r\nBad command");

	} else {
		printf("\r\nBad Actor");
	}
	return argc;
}
