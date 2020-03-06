#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#include "argoat.h"
#include "bool.h"
#include "db.h"
#include "dirs.h"
#include "lcharmap.h"
#include "terminfo.h"
#include "types.h"
#include "util.h"

int
main(int argc, char **argv)
{
	/* load database */
	char dbpath[4091];
	
	char *datadir = dirs_data_dir();
	if (datadir == NULL) {
		fprintf(stderr, "lcharmap: error: unable to determine location of character database.\n");
		return 1;
	}

	sprintf(&dbpath, "%s%clcharmap%cchars.db", datadir, pathsep(), pathsep());
	db = chardb_open((char*) &dbpath);

	/* set default options */
	opts = (struct Options*) malloc(1 * sizeof(struct Options));
	if (opts == NULL) {
		fprintf(stderr, "lcharmap: error: unable to allocate memory:");
		perror(NULL);
		return 1;
	}

	opts->format_long = FALSE;
	opts->ttywidth    = ttywidth();

	/* parse arguments with cylgom/argoat */
	const struct argoat_sprig sprigs[15] = {
		/* unflagged */
		{ NULL,      0, NULL,                       NULL         },
		{ "version", 0, NULL,                       version      },
		{ "V",       0, NULL,                       version      },
		/* -v instead of -V will be removed soon */
		{ "v",       0, NULL,                       version      },
		{ "help",    0, NULL,                       usage        },
		{ "h",       0, NULL,                       usage        },
		{ "long",    0, (void*) &opts->format_long, handle_bool  },
		{ "l",       0, (void*) &opts->format_long, handle_bool  },
		{ "range",   1, NULL,                       range        },
		{ "r",       1, NULL,                       range        },
		{ "rage",    0, NULL,                       handle_anger },
		{ "chars",   1, NULL,                       chars        },
		{ "c",       1, NULL,                       chars        },
		{ "search",  1, NULL,                       search       },
		{ "s",       1, NULL,                       search       },
	};

	struct argoat args = { sprigs, sizeof(sprigs), NULL, 0, 0 };
	argoat_graze(&args, argc, argv);

	free(opts);
}

void
range(void *data, char **pars, const int pars_count)
{
	if (pars_count < 1) {
		fprintf(stderr, "lcharmap: error: argument to --range missing.\n");
		exit(0);
	}

	usize range[2];
	char *range1 = strsep(&pars[0], ",");
	char *range2 = strsep(&pars[0], ",");

	usize base = 10;
	if (!strncmp(range1, "0x", 2))
		base = 16;
	else if (!strncmp(range1, "0", 1))
		base = 8;

	range[0] = strtol(range1, NULL, base);

	if (range2 != NULL) {
		base = 10;
		if (!strncmp(range2, "0x", 2))
			base = 16;
		else if (!strncmp(range2, "0", 1))
			base = 8;
		range[1] = strtol(range2, NULL, base);
	}

	/* print range */
	/* TODO: merge this code with above */
	if (range2 == NULL) {
		print_entry_long(range[0], chardb_getdesc(db, range[0]));
		return;
	}

	if (range[1] < range[0]) {
		fprintf(stderr, "lcharmap: error: provided range %i -> %i doesn't make sense.\n",
			range[0], range[1]);
		exit(1);
	}

	if (range[1] == range[0]) {
		print_entry_long(range[0], chardb_getdesc(db, range[0]));
		return;
	}

	if (opts->format_long) {
		for (usize i = range[0]; i <= range[1]; ++i) {
			print_entry_long(i, chardb_getdesc(db, i));
			printf("\n");
		}
	} else {
		print_line(opts->ttywidth);
		print_header();
		print_line(opts->ttywidth);

		for (usize i = range[0]; i <= range[1]; ++i) {
			print_entry_short(i, chardb_getdesc(db, i));
		}
	}
}

void
chars(void *data, char **pars, const int pars_count)
{
	if (pars_count < 1) {
		fprintf(stderr, "lcharmap: error: argument to --chars missing.\n");
		exit(1);
	}

	/* TODO: unicode support, currently UTF8 continuation chars will be
	 * counted as an extra character */
	char *chars = pars[0];
	usize len = strlen(pars[0]);

	/* TODO: deduplicate and sort string */

	if (len > 1 && !opts->format_long) {
		print_line(opts->ttywidth);
		print_header();
		print_line(opts->ttywidth);
	}

	for (usize i = 0; i < len; ++i) {
		if (len < 2 || opts->format_long) {
			print_entry_long(chars[i], chardb_getdesc(db, chars[i]));

			/* line padding before next entry */
			printf("\n");
		} else {
			print_entry_short(chars[i], chardb_getdesc(db, chars[i]));
		}
	}
}

void
search(void *data, char **pars, const int pars_count)
{
	if (pars_count < 1) {
		fprintf(stderr, "lcharmap: error: argument to --search missing.\n");
		exit(1);
	}

	char *query = pars[0];
	regex_t re;
	isize err = regcomp(&re, query, 0);

	/* TODO: reusable die() function */
	if (err) {
		/* TODO: get char of error and error message */
		fprintf(stderr, "lcharmap: error: '%s': invalid regex query.\n", query);
		exit(1);
	}

	/* TODO: malloc as needed */
	u32 matches[32841];
	usize match_count = chardb_search(db, &re, &matches);

	if (match_count > 1 && !opts->format_long) {
		print_line(opts->ttywidth);
		print_header();
		print_line(opts->ttywidth);
	}

	for (usize i = 0; i < match_count; ++i) {
		if (match_count < 2 || opts->format_long) {
			print_entry_long(matches[i], chardb_getdesc(db, matches[i]));

			/* line padding before next entry */
			printf("\n");
		} else {
			print_entry_short(matches[i], chardb_getdesc(db, matches[i]));
		}
	}
}

void
print_entry_short(u32 entry, char *description)
{
	/* don't print control characters */
	if (entry < 32) {
		printf("%i\t%X\t%o\t&#%i;\t \t%s\n", entry, entry, entry,
				entry, description);
	} else {
		printf("%i\t%X\t%o\t&#%i;\t%c\t%s\n", entry, entry, entry,
				entry, entry, description);
	}

	print_line(opts->ttywidth);
}

void
print_entry_long(u32 entry, char *description)
{
	char dec[snprintf(NULL, 0, "%d", entry)];
	sprintf(&dec, "%d", entry);

	char hex[snprintf(NULL, 0, "%X", entry)];
	sprintf(&hex, "%X", entry);

	char oct[snprintf(NULL, 0, "%o", entry)];
	sprintf(&oct, "%o", entry);

	/*
	 * TODO: display "readable" html entities
	 * e.g. &amp; instead for '&' instead of &#38;
	 */
	char htm[snprintf(NULL, 0, "&#%d;", entry)];
	sprintf(&htm, "&#%d;", entry);

	char cha[snprintf(NULL, 0, "%c", entry)];
	sprintf(&cha, "%c", entry);

	print_entry_row("decimal", &dec);
	print_entry_row("hexadecimal", &hex);
	print_entry_row("octal", &oct);
	print_entry_row("HTML entity", &htm);
	print_entry_row("character", &cha);
	print_entry_row("description", description);
}

void
print_entry_row(char *key, char *val)
{
	usize keylen = strlen(key);
	for (usize i = 0; i < (LONG_FORMAT_PADDING - keylen); ++i)
		printf(" ");
	printf("%c[1m%s%c[0m  %s\n", ESCAPE, key, ESCAPE, val);
}

void
handle_bool(void *data, char **pars, const int pars_count)
{
	*((bool*) data) = TRUE;
}

/* ¯\_(ツ)_/¯ */
void
handle_anger(void *data, char **pars, const int pars_count)
{
	fprintf(stderr, "Grrr\n");
	exit(0);
}

void
version(void *data, char **pars, const int pars_count)
{
	fprintf(stdout, "lcharmap %s\n", VERSION);
	exit(0);
}

void
usage(void *data, char **pars, const int pars_count)
{
	printf("Usage: lcharmap [OPTIONS] [ARGS]\n");
	printf("Print and search information on the provided Unicode characters.\n\n");
	printf("OPTIONS:\n");
	printf("    -l, --long          print character entries in the long format.\n");
	printf("    -h, --help          print this help message and exit.\n");
	printf("    -V, --version       print version and exit.\n\n");
	printf("ARGUMENTS:\n");
	printf("    -r, --range RANGE   print a range of Unicode characters.\n");
	printf("    -c, --chars CHARS   print a range of Unicode codepoints that match\n");
	printf("                        provided character(s).\n");
	printf("    -s, --search TERM   search character descriptions for TERM.\n\n");
	printf("EXAMPLES:\n");
	printf("    lcharmap -r 0,128   print information for characters from 0 to 128.\n");
	printf("    lcharmap -c abcde   print information for characters 'a', 'b', 'c',\n");
	printf("                        'd', and 'e'.\n");
	printf("    lcharmap -s latin   search characters for description 'latin'.\n");
	printf("                        regular expressions are supported.\n\n");
	printf("Report issues to https://github.com/lptstr/lcharmap.\n");
	printf("Full documentation is available locally at lcharmap(1).\n");
	exit(0);
}
