#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"

/*
  "The prompt should indicate the absolute working directory, the user
  name, and the machine name. You can do this by expanding the $USER,
  $MACHINE, $PWD environment variables." Tilde Expansion: "In Bash, tilde
  (~) may appear at the beginning of a path, and it expands to the
  environment variable $HOME." Path Search: "When you enter “ls” in
  Bash, the shell executes the program at '/usr/bin/ls'. This is a
  simple search of a predefined list of directories. This list of
  directories is defined in the environment variable called $PATH.
  When the command does not include a (/) or is not a built-in function
  (part 10), you will need to search every directory specified in $PATH.
  $PATH is just a string where the directories are delimited by a colon.
  So you will need to perform some string operations.
  */


tokenlist *get_tokens(char *input);

tokenlist *new_tokenlist(void);
void add_token(tokenlist *tokens, char *item);


tokenlist *parser(char *img_name)
{
	// PROMPT:
	printf("%s > ", img_name);

	/* "input" contains the whole command
	 * "tokens" contains substrings from input, split by spaces */

	char *input = get_input(); // reads standard in

	tokenlist *tokens = get_tokens(input);

	return(tokens);
}


tokenlist *new_tokenlist(void)
{
	tokenlist *tokens = (tokenlist *) malloc(sizeof(tokenlist));
	tokens->size = 0;
	tokens->items = (char **) malloc(sizeof(char *));
	tokens->items[0] = NULL; /* make NULL terminated */
	return tokens;
}

void add_token(tokenlist *tokens, char *item)
{
	int i = tokens->size;

	tokens->items = (char **) realloc(tokens->items, (i + 2) * sizeof(char *));
	tokens->items[i] = (char *) malloc(strlen(item) + 1);
	tokens->items[i + 1] = NULL;
	strcpy(tokens->items[i], item);

	tokens->size += 1;
}

char *get_input(void)
{
	char *buffer = NULL;
	int bufsize = 0;

	char line[5];
	// read stdin 5 characters at a time and store them in 'line'
	// stop when the input is empty
	while (fgets(line, 5, stdin) != NULL) {
		int addby = 0;
		char *newln = strchr(line, '\n');
		if (newln != NULL)
			addby = newln - line;
		else
			addby = 5 - 1;

		buffer = (char *) realloc(buffer, bufsize + addby);
		memcpy(&buffer[bufsize], line, addby);
		bufsize += addby;

		if (newln != NULL)
			break;
	}

	buffer = (char *) realloc(buffer, bufsize + 1);
	buffer[bufsize] = 0;

	return buffer;
}

tokenlist *get_tokens(char *input)
{
	char *buf = (char *) malloc(strlen(input) + 1);
	strcpy(buf, input);

	tokenlist *tokens = new_tokenlist();

	char *tok = strtok(buf, " ");
	while (tok != NULL) {
		add_token(tokens, tok);
		tok = strtok(NULL, " ");
	}

	free(buf);
	return tokens;
}

void free_tokens(tokenlist *tokens)
{
	int i;
	for (i = 0; i < tokens->size; i++)
		free(tokens->items[i]);
	free(tokens->items);
	free(tokens);
}
