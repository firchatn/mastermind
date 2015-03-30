#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifndef DISABLE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif // DISABLE_READLINE

#include "../config.h"
#include "cli-cmd.h"
#include "core.h"

#define LEN(a) (sizeof(a) / sizeof(a[0]))
mm_session *session;
cmd_t cmds[] = {
    {.n = "quit", .e = cmd_quit},
    {.n = "set", .e = cmd_set},
    {.n = "restart", .e = cmd_restart},
    {.n = "savegame", .e = cmd_savegame},
    {.n = "score", .e = cmd_score},
    {.n = "reset", .e = cmd_reset},
    {.n = "help", .e = cmd_help},
    {.n = "account", .e = cmd_account},
    {.n = "version", .e = cmd_version},
}; // "connect", "server", "disconnect", "version"
void printPanel()
{
	unsigned i, j;
	for (i = 0; i < session->config->guesses; i++) {
		putchar('+');
		for (j = 0; j < session->config->holes; j++)
			printf("---+");
		printf("  +-----+-----+-----+\n|");
		for (j = 0; j < session->config->holes; j++)
			if (i < session->guessed)
				printf(" %d |",
				       session->panel[i].combination[j]);
			else
				printf("   |");
		if (i < session->guessed)
			printf(_("  | Y%2d | A%2d | N%2d |\n"),

			       session->panel[i].inplace,
			       session->panel[i].insecret -
				   session->panel[i].inplace,
			       session->config->holes -
				   session->panel[i].insecret);
		else
			printf("  |     |     |     |\n");
	}
	putchar('+');
	for (j = 0; j < session->config->holes; j++)
		printf("---+");
	printf("  +-----+-----+-----+");
	putchar('\n');
}
#ifdef WINDOWS
char *strndup(const char *buf, size_t len)
{
	size_t i = strlen(buf);
	if (i < len)
		len = i;
	char *s = (char *)malloc(sizeof(char) * (len + 1));
	for (i = 0; i < len; i++)
		s[i] = buf[i];
	s[i] = '\0';
	return s;
}
#endif
char **parseBuf(char *buf, unsigned *argc)
{
	char *start = NULL;
	char **args, *c;
	unsigned i = 0;
	c = buf;
	// get argc
	while (*c != '\0') {
		if (*c == ' ' || *c == '\t' || *c == ',' || *c == '\n') {
			if (start) {
				i++;
				start = NULL;
			}
		} else if ((*c >= '0' && *c <= '9') ||
			   (*c >= 'a' && *c <= 'z') || *c == '_') {
			if (start == NULL)
				start = c;
		} else {
			printf(_("\nError: illegal charater on the command "
				 "'%c'\n"),
			       *c);
			i = 0;
			start = NULL;
			break;
		}
		c++;
	}
	if (start)
		i++;
	*argc = i;
	// get args
	if (*argc == 0)
		return (char **)NULL;
	args = (char **)malloc(sizeof(char *) * (*argc));
	i = 0;
	start = NULL;
	while (*buf != '\0') {
		if (*buf == ' ' || *buf == '\t' || *buf == ',' ||
		    *buf == '\n') {
			if (start) {
				args[i++] = strndup(start, buf - start);
				start = NULL;
			}
		} else {
			if (start == NULL)
				start = buf;
		}
		buf++;
	}
	if (start)
		args[i++] = strndup(start, buf - start);
	return args;
}
#ifndef DISABLE_READLINE
static char **completeCombination(const char *txt, int start, int end)
{
	if (rl_point != rl_end)
		return (char **)NULL;
	unsigned l = 0, j, i, argc;
	char *output = NULL, **args;
	cmd_t *cmd, *cmpltCmd = NULL;
	mm_conf_t *conf, *cmpltCnf = NULL;
	char **T = (char **)malloc(sizeof(char *) *
				   (LEN(cmds) + session->config->colors + 2));
	T[l++] = strdup("");
	args = parseBuf(rl_line_buffer, &argc);
	if (argc == 0)
		for (cmd = cmds; cmd < cmds + LEN(cmds); cmd++, l++)
			T[l] = strdup(cmd->n);
	if (argc == 0 || (args[0][0] >= '0' &&
			  args[0][0] < ('0' + session->config->colors))) {
		j = 0;
		for (i = 0; i < strlen(rl_line_buffer); i++)
			if (rl_line_buffer[i] >= '0' &&
			    rl_line_buffer[i] < '0' + session->config->colors)
				j++;
			else if (rl_line_buffer[i] != ' ' &&
				 rl_line_buffer[i] != '\t' &&
				 rl_line_buffer[i] != ',')
				goto no_more;
		if (j >= session->config->holes)
			goto no_more;
		for (j = 0; j < session->config->colors; j++, l++) {
			T[l] = (char *)malloc(sizeof(char) * 2);
			sprintf(T[l], "%u", j);
		}
	}
	if (argc == 1 && args[0][0] >= 'a' && args[0][0] <= 'z') {
		j = l;
		for (cmd = cmds; cmd < cmds + LEN(cmds); cmd++) {
			if (strstr(cmd->n, args[0]) == cmd->n) {
				if (strcmp(cmd->n, args[0]) == 0)
					cmpltCmd = cmd;
				else
					T[l++] = strdup(cmd->n);
			}
		}
		if (l == j + 1)
			output = T[j];
	}
	if (cmpltCmd && strcmp(cmpltCmd->n, "set") == 0) {
		for (conf = mm_confs; conf < mm_confs + LEN(mm_confs);
		     conf++, l++)
			T[l] = strdup(conf->nm);
	}
	if (argc == 2 && strcmp(args[0], "set") == 0) {
		j = l;
		for (conf = mm_confs; conf < mm_confs + LEN(mm_confs); conf++) {
			if (strstr(conf->nm, args[1]) == conf->nm) {
				if (strcmp(conf->nm, args[1]) == 0)
					cmpltCnf = conf;
				else
					T[l++] = strdup(conf->nm);
			}
		}
		if (l == j + 1)
			output = T[j];
		if (cmpltCnf) {
			output = (char *)malloc(sizeof(char) * 4);
			sprintf(output, "%d", cmpltCnf->val);
		}
	}
no_more:
	if (output) {
		T[0] = strdup(output);
		for (i = 1; i < l; i++)
			free(T[i]);
		l = 1;
		if (cmpltCnf)
			free(output);
	} else if (l == 1 && rl_line_buffer[rl_point - 1] != ' ') {
		free(T[0]);
		free(T);
		T = (char **)NULL;
	}
	if (T)
		T[l++] = NULL;
	while (argc--)
		free(args[argc]);
	free(args);
	return T;
}
#endif // DISABLE_READLINE

/* return:
* -1 : input error, redo (do not redraw table)
*  0 : seccess input, redo if mm_play(T) does not success (do not redraw
*      table) or next (redraw table)
* 1  : cmd input, redo (do not redo table)
* 2  : cmd input, next (redraw table)
* */
int getCombination(uint8_t *T)
{
	unsigned ret = -1;
	char prmpt[200], *input, **args;
	unsigned i = 0, j = 0, k, argc;
	uint8_t c;
	cmd_t *cmd;
	snprintf(prmpt, 200, _("Enter you guesse: (%d of [0..%d] nbre) > "),
		 session->config->holes, session->config->colors - 1);
#ifndef DISABLE_READLINE
	rl_attempted_completion_function = completeCombination;
	while ((input = readline(prmpt)) == NULL) {
	};
#else
	printf("%s", prmpt);
	input = (char *)malloc(sizeof(char) * 4096);
	fgets(input, 4095, stdin);
#endif // DISABLE_READLINE
	args = parseBuf(input, &argc);
	if (argc == 0)
		goto input_err;
	if (args[0][0] >= 'a' && args[0][0] <= 'z') {
		cmd = cmds;
		while (cmd < cmds + LEN(cmds) &&
		       strstr(cmd->n, args[0]) != cmd->n)
			cmd++;
		if (cmd < cmds + LEN(cmds)) {
			ret = cmd->e(argc, (const char **)args, session);
#ifndef DISABLE_READLINE
			add_history(strdup(input));
#endif // DISABLE_READLINE
			ret = ret == 1 ? 2 : 1;
		} else {
			printf(_("Command unsupported '%s'\n"), args[0]);
			goto input_err;
		}
		goto cmd_execed;
	}
	i = 0;
	j = 0;
	k = 0;
	while ((i < session->config->holes) &&
	       (args[j][k] != '\0' || (++j < argc && !(k = 0)))) {
		c = args[j][k++];
		if (c >= '0' && c <= '9') {
			T[i++] = c - '0';
		} else if (c == ' ' || c == ',' || c == ';' || c == '\n') {
			continue;
		} else {
			printf(_("Illigal char on you guesse!!\n"));
			goto parse_err;
		}
	}
#ifndef DISABLE_READLINE
	if (T)
		add_history(input);
	else
#endif // DISABLE_READLINE
		free(input);
	while (argc--)
		free(args[argc]);
	free(args);
	return 0;
parse_err:
input_err:
cmd_execed:
	free(input);
	while (argc--)
		free(args[argc]);
	free(args);
	return ret;
}
int main()
{
	uint8_t *T = NULL, c;
	unsigned ret;
#ifndef DISABLE_LOCALE
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif
	session = mm_session_restore();
	do {
		if (!session) {
			printf(_("Starting new session\n"));
			session = mm_session_new();
		} else {
			printf(_("Restoring old session\n"));
		}
		printf(_("Current Config:\n\tguesses: %d\n\tcolors: "
			 "%d\n\tholes: %d\n\tremise: %d\n"),
		       session->config->guesses, session->config->colors,
		       session->config->holes, session->config->remise);
		do {
			printf(_("Current State:\n\tGuessed: %d\n"),
			       session->guessed);
			printPanel();
			T = (uint8_t *)malloc(sizeof(uint8_t) *
					      session->config->holes);
			while ((ret = getCombination(T)) == -1 || ret == 1 ||
			       (ret == 0 && mm_play(session, T) != 0)) {
				if (ret == 0) {
					printf(
					    _("You Guesse is not valid!!\n"));
				}
			}
			if (ret != 0)
				free(T);
		} while (session->state == MM_PLAYING ||
			 session->state == MM_NEW);
		printf(_("Final State:\n"));
		printPanel();
		printf("%s\n", (session->state == MM_SUCCESS)
				   ? _("You successed :)")
				   : _("You Failed :("));
		if (session->state == MM_SUCCESS)
			printf(_("Your score is: %ld\n"), mm_score(session));
		printf(_("Secret Key is: "));
		for (c = 0; c < session->config->holes; c++)
			putchar('0' + session->secret->val[c]);
		putchar('\n');
		mm_session_free(session);
		session = NULL;
		printf(_("restart? (Y/N): "));
		scanf("%c", &c);
	} while (c == 'y' || c == 'Y');
	return 0;
}
