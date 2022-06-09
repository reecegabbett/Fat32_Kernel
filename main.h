#ifndef FUNCTIONS_H_INCLUDED
#define FUNCTIONS_H_INCLUDED

/* Types */
typedef struct {
	int size;
	char **items;
} tokenlist;

/* Declarations */

// Parser
extern tokenlist *parser();
extern void free_tokens(tokenlist *tokens);
char *get_input(void);
extern tokenlist *get_tokens(char *input);
extern tokenlist *new_tokenlist(void);

#endif
