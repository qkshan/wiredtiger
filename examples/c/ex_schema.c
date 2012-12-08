/*-
 * Public Domain 2008-2012 WiredTiger, Inc.
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * ex_schema.c
 *	This is an example application demonstrating how to create and access
 *	tables using a schema.
 */

#include <stdio.h>
#include <string.h>

#include <inttypes.h>
#include <wiredtiger.h>

const char *home = "WT_TEST";

/*! [schema declaration] */
/* The C struct for the data we are storing in a WiredTiger table. */
typedef struct {
	char country[5];
	unsigned short year;
	unsigned long long population;
} POP_RECORD;

POP_RECORD pop_data[] = {
	{ "AU",  1900,	  4000000 },
	{ "AU",  2000,	 19053186 },
	{ "CAN", 1900,	  5500000 },
	{ "CAN", 2000,	 31099561 },
	{ "UK",  1900,	369000000 },
	{ "UK",  2000,	 59522468 },
	{ "USA", 1900,	 76212168 },
	{ "USA", 2000,	301279593 },
	{ }
};
/*! [schema declaration] */

int
main(void)
{
	POP_RECORD *p;
	WT_CONNECTION *conn;
	WT_CURSOR *cursor;
	WT_SESSION *session;
	unsigned long long population;
	uint64_t recno;
	unsigned short year;
	const char *country;
	int ret;

	ret = wiredtiger_open(home, NULL, "create", &conn);
	if (ret != 0)
		fprintf(stderr, "Error connecting to %s: %s\n",
		    home, wiredtiger_strerror(ret));
	/* Note: error checking omitted for clarity. */

	ret = conn->open_session(conn, NULL, NULL, &session);

	/*! [Create a table with column groups] */
	/*
	 * Create the population table.
	 * Keys are record numbers, the format for values is (5-byte string,
	 * unsigned short, unsigned long long).
	 * See ::wiredtiger_struct_pack for details of the format strings.
	 */
	ret = session->create(session, "table:mytable",
	    "key_format=r,"
	    "value_format=5sHQ,"
	    "columns=(id,country,year,population),"
	    "colgroups=(main,population)");

	/*
	 * Create two column groups: a primary column group with the country
	 * code, year and population (named "main"), and a population column
	 * group with the population by itself (named "population").
	 */
	ret = session->create(session,
	    "colgroup:mytable:main", "columns=(country,year,population)");
	ret = session->create(session,
	    "colgroup:mytable:population", "columns=(population)");
	/*! [Create a table with column groups] */

	/*! [Create an index] */
	/* Create an index with a simple key. */
	ret = session->create(session,
	    "index:mytable:country", "columns=(country)");
	/*! [Create an index] */

	/*! [Create an index with a composite key] */
	/* Create an index with a composite key (country,year). */
	ret = session->create(session,
	    "index:mytable:country_plus_year", "columns=(country,year)");
	/*! [Create an index with a composite key] */

	/* Insert the records into the table. */
	ret = session->open_cursor(
	    session, "table:mytable", NULL, "append", &cursor);
	for (p = pop_data; p->year != 0; p++) {
		cursor->set_value(cursor, p->country, p->year, p->population);
		ret = cursor->insert(cursor);
	}
	ret = cursor->close(cursor);

	/* List the records in the table. */
	ret = session->open_cursor(session,
	    "table:mytable", NULL, NULL, &cursor);
	while ((ret = cursor->next(cursor)) == 0) {
		cursor->get_key(cursor, &recno);
		cursor->get_value(cursor, &country, &year, &population);
		printf("ID %" PRIu64 ": country %s, year %u, population %llu\n",
		    recno, country, (unsigned int)year, population);
	}
	ret = cursor->close(cursor);

	/*! [Read population from the primary column group] */
	/*
	 * Open a cursor on the main column group, and return the information
	 * for a particular country.
	 */
	ret = session->open_cursor(
	    session, "colgroup:mytable:main", NULL, NULL, &cursor);
	cursor->set_key(cursor, 2);
	ret = cursor->search(cursor);
	cursor->get_value(cursor, &country, &year, &population);
	printf("ID 2: country %s, year %u, population %llu\n",
	    country, (unsigned int)year, population);
	/*! [Read population from the primary column group] */
	ret = cursor->close(cursor);

	/*! [Read population from the standalone column group] */
	/*
	 * Open a cursor on the population column group, and return the
	 * population of a particular country.
	 */
	ret = session->open_cursor(session,
	    "colgroup:mytable:population", NULL, NULL, &cursor);
	cursor->set_key(cursor, 2);
	ret = cursor->search(cursor);
	cursor->get_value(cursor, &population);
	printf("ID 2: population %llu\n", population);
	/*! [Read population from the standalone column group] */
	ret = cursor->close(cursor);

	/*! [Search in a simple index] */
	/* Search in a simple index. */
	ret = session->open_cursor(session,
	    "index:mytable:country", NULL, NULL, &cursor);
	cursor->set_key(cursor, "AU\0\0\0");
	ret = cursor->search(cursor);
	cursor->get_value(cursor, &country, &year, &population);
	printf("AU: country %s, year %u, population %llu\n",
	    country, (unsigned int)year, population);
	/*! [Search in a simple index] */
	ret = cursor->close(cursor);

	/*! [Search in a composite index] */
	/* Search in a composite index. */
	ret = session->open_cursor(session,
	    "index:mytable:country_plus_year", NULL, NULL, &cursor);
	cursor->set_key(cursor, "USA\0\0", (unsigned short)1900);
	ret = cursor->search(cursor);
	cursor->get_value(cursor, &country, &year, &population);
	printf("US 1900: country %s, year %u, population %llu\n",
	    country, (unsigned int)year, population);
	/*! [Search in a composite index] */
	ret = cursor->close(cursor);

	/*! [Return the table's record number key using an index] */
	/* Return the table's record number key using an index. */
	ret = session->open_cursor(session,
	    "index:mytable:country_plus_year(id)", NULL, NULL, &cursor);
	while ((ret = cursor->next(cursor)) == 0) {
		cursor->get_key(cursor, &country, &year);
		cursor->get_value(cursor, &recno);
		printf(
		    "row ID %" PRIu64 ": country %s, year %u\n",
		    recno, country, year);
	}
	/*! [Return the table's record number key using an index] */
	ret = cursor->close(cursor);

	/*! [Return a subset of the value columns from an index] */
	/* Return the record number of the table entries using an index. */
	ret = session->open_cursor(session,
	    "index:mytable:country_plus_year(population)", NULL, NULL, &cursor);
	while ((ret = cursor->next(cursor)) == 0) {
		cursor->get_key(cursor, &country, &year);
		cursor->get_value(cursor, &population);
		printf("population %llu: country %s, year %u\n",
		    population, country, year);
	}
	/*! [Return a subset of the value columns from an index] */
	ret = cursor->close(cursor);

	/*! [Read the record number column from the composite index] */
	/* Return the record number of the table entries using an index. */
	ret = session->open_cursor(session,
	    "index:mytable:country_plus_year(id)", NULL, NULL, &cursor);
	while ((ret = cursor->next(cursor)) == 0) {
		cursor->get_key(cursor, &country, &year);
		cursor->get_value(cursor, &recno);
		printf(
		    "row ID %llu: country %s, year %u\n", recno, country, year);
	}
	/*! [Read a subset of information from the composite index] */
	ret = cursor->close(cursor);
	/*! [schema complete] */

	ret = conn->close(conn, NULL);

	return (ret);
}
