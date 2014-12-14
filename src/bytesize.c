#include <stdlib.h>
#include <string.h>
#include <ctype.h>


#define BYTESIZE_METRICS "BKMGT"


/** Get byte size representation value from string.
 *
 * If `str` does not point to a valid byte size representation,
 * the bytesize() function returns -1.
 *
 * Example:
 *      long size = bytesize("32 KB");
 */
long        bytesize(const char *str)
{
    long    result;
    char    *endptr;
    char    metrics[] = "BKMGT";
    int     c;

    result = strtol(str, &endptr, 10);
    if (result < 0 || endptr == str)
    {
        return (-1L);
    }
    while (*endptr == ' ' || *endptr == '\t')
    {
        endptr++;
    }
    c = toupper(*endptr);
    if (c == '\0' || c == 'O')
    {
        c = 'B';
    }
    str = strchr(metrics, c);
    if (str == NULL)
    {
        return (-1L);
    }
    c = str - metrics;
    while (c--)
    {
        result *= 1024;
    }
    return (result);
}
