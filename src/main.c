#include <err.h>
#include <regex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>
#include <unistd.h>

#include "arg.h"
#include "display.c"
#include "util.h"
#include "range.c"
#include "unicode.h"

sqlite3 *db;
_Bool istty = false;
_Bool flong = false;

static void
range(char *param)
{
	/*
	 * TODO: support number characters from other languages
	 * e.g. Chinese
	 */

	uint32_t entries[262144];
	ssize_t entries_len = -1;

	if ((entries_len = expand_range(param, entries)) < 0)
		errx(1, "'%s': invalid range.", param);

	printheader(flong, istty);

	for (size_t i = 0; i < (size_t)entries_len; ++i) {
		if (entries[i] > UNICODE_MAX) {
			warnx("%u is above maximum Unicode value", entries[i]);
			continue;
		}
		char *desc = charinfos[entries[i]].desc;
		printentry(entries[i], desc, istty, flong);
	}
}

static void
chars(char *param)
{
	char *inp = param;
	size_t len = strlen(inp);

	printheader(flong, istty);

	while (*inp) {
		size_t   offset = inp - param;
		uint32_t charbuf = 0;
		ssize_t  runelen = utf8_decode(&charbuf, inp, len - offset);

		if (runelen < 0) {
			warnx("invalid UTF8 rune at offset %zu", offset);
			++inp;
			continue;
		}

		printentry(charbuf, charinfos[charbuf].desc, istty, flong);
		inp += runelen;
	}
}

static void
search(char *query)
{
	regex_t re;

	/* TODO: get char of error and error message */
	if (regcomp(&re, query, REG_ICASE))
		errx(1, "'%s': invalid regex.", query);

	printheader(flong, istty);

	for (size_t i = 0; i < UNICODE_MAX; ++i) {
		char *desc = charinfos[i].desc;
		if (desc == NULL)
			continue;
		if (regexec(&re, desc, 0, NULL, 0) != REG_NOMATCH)
			printentry(i, desc, istty, flong);
	}

	regfree(&re);
}

static void
usage(_Bool _short)
{
	printf("Usage: chmap [-C always|never|auto] [-l] [-r RANGE] [-c CHARS] [-s REGEX]\n");

	if (_short)
		exit(0);

	printf("\n");
	printf("Print information for Unicode characters.\n");
	printf("\n");
	printf("OPTIONS:\n");
	printf("    -l, --long          Show character entries in the long format.\n");
	printf("    -h, --help          print this help message and exit.\n");
	printf("    -V, --version       print version and exit.\n");
	printf("\n");
	printf("FLAGS:\n");
	printf("    -r, --range RANGE   print a range of Unicode characters.\n");
	printf("    -c, --chars CHARS   print a range of Unicode codepoints that match\n");
	printf("                        provided character(s).\n");
	printf("    -s, --search REGEX  search character descriptions for REGEX.\n");
	printf("\n");
	printf("Full documentation is available locally at chmap(1).\n");

	exit(0);
}

static _Bool
usecolor(void)
{
	if (!isatty(STDOUT_FILENO))
		return false;

	char *env_NOCOLOR = getenv("NO_COLOR");
	char *env_TERM = getenv("TERM");

	if (env_NOCOLOR)
		return false;

	if (!env_TERM || !strcmp(env_TERM, "dumb"))
		return false;

	return true;
}

int
main(int argc, char **argv)
{
	istty = usecolor();

	ARGBEGIN {
	break; case 'l':
		flong = !flong;
	break; case 'r':
		range(EARGF(usage(true)));
	break; case 'c':
		chars(EARGF(usage(true)));
	break; case 's':
		search(EARGF(usage(true)));
	break; case 'C':
		optarg = EARGF(usage(true));
		if (!strncmp(optarg, "au", 2))
			istty = usecolor();
		else if (!strncmp(optarg, "al", 2))
			istty = true;
		else if (!strncmp(optarg, "ne", 2))
			istty = false;
		else
			usage(true);
	break; case 'V': case 'v':
		printf("chmap v%s\n", VERSION);
		return 0;
	break; case 'h':
		usage(false);
	break; default:
		usage(true);
	} ARGEND
}
