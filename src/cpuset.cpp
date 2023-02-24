/* cpuset_t parsing
 * Modified from lib/cpuset.c of util-linux
 */

#include <errno.h>
#include <unistd.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cpuset.h"

namespace {

#define cpuset_nbits(setsize)	(8 * (setsize))

static inline int val_to_char(int v)
{
	if (v >= 0 && v < 10)
		return '0' + v;
	if (v >= 10 && v < 16)
		return ('a' - 10) + v;
	return -1;
}

static inline int char_to_val(int c)
{
	int cl;

	if (c >= '0' && c <= '9')
		return c - '0';
	cl = tolower(c);
	if (cl >= 'a' && cl <= 'f')
		return cl + (10 - 'a');
	return -1;
}

static const char *nexttoken(const char *q,  int sep)
{
	if (q)
		q = strchr(q, sep);
	if (q)
		q++;
	return q;
}

static int nextnumber(const char *str, char **end, unsigned int *result)
{
	errno = 0;
	if (str == NULL || *str == '\0' || !isdigit(*str))
		return -EINVAL;
	*result = (unsigned int) strtoul(str, end, 10);
	if (errno)
		return -errno;
	if (str == *end)
		return -EINVAL;
	return 0;
}

}

bool CpusetParse(const char *str, cpu_set_t *set, size_t ncpu) {
	const char *p, *q;
	char *end = NULL;

	q = str;
	CPU_ZERO(set);
  
  if (strcmp(str, "all") == 0) {
    for (unsigned int i = 0; i < ncpu; i++) CPU_SET(i, set);
    return true;
  }
  if (strcmp(str, "none") == 0) return true;

	while (p = q, q = nexttoken(q, ','), p) {
		unsigned int a;	/* beginning of range */
		unsigned int b;	/* end of range */
		unsigned int s;	/* stride */
		const char *c1, *c2;

		if (nextnumber(p, &end, &a) != 0)
			return false;
		b = a;
		s = 1;
		p = end;

		c1 = nexttoken(p, '-');
		c2 = nexttoken(p, ',');

		if (c1 != NULL && (c2 == NULL || c1 < c2)) {
			if (nextnumber(c1, &end, &b) != 0)
				return false;

			c1 = end && *end ? nexttoken(end, ':') : NULL;

			if (c1 != NULL && (c2 == NULL || c1 < c2)) {
				if (nextnumber(c1, &end, &s) != 0)
					return false;
				if (s == 0)
					return false;
			}
		}

		if (!(a <= b))
			return false;
		while (a <= b && a < ncpu) {
			if (a < ncpu) CPU_SET(a, set);
			a += s;
		}
	}

	if (end && *end)
		return false;
	return true;
}