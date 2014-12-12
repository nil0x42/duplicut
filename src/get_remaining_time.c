#include <stdio.h>
#include <time.h>
#include <string.h>
#include "vars.h"


#define BUF_SIZE (200)
#define MINUTE   (60)
#define HOUR     (MINUTE * 60)
#define DAY      (HOUR * 24)
char            *time_repr(int seconds)
{
    static char buf[BUF_SIZE];
    char        *ptr;
    int         metrics;
    int         unit;
    char        *repr;

    metrics = 0;
    memset(buf, 0, BUF_SIZE);
    ptr = buf;
    ptr += sprintf(ptr, "(");
    if (metrics < 2 && seconds >= DAY)
    {
        unit = seconds / DAY;
        repr = (unit == 1) ? "day" : "days";
        ptr += sprintf(ptr, "%d %s", unit, repr);
        seconds %= DAY;
        metrics++;
    }
    if (metrics < 2 && seconds >= HOUR)
    {
        if (metrics == 1)
        {
            ptr += sprintf(ptr, ", ");
        }
        unit = seconds / HOUR;
        repr = (unit == 1) ? "hour" : "hours";
        ptr += sprintf(ptr, "%d %s", unit, repr);
        seconds %= HOUR;
        metrics++;
    }
    if (metrics < 2 && seconds >= MINUTE)
    {
        if (metrics == 1)
        {
            ptr += sprintf(ptr, ", ");
        }
        unit = seconds / MINUTE;
        repr = (unit == 1) ? "minute" : "minutes";
        ptr += sprintf(ptr, "%d %s", unit, repr);
        seconds %= MINUTE;
        metrics++;
    }
    if (metrics < 2 && seconds >= 0)
    {
        if (metrics == 1)
        {
            ptr += sprintf(ptr, ", ");
        }
        repr = (unit == 1) ? "second" : "seconds";
        ptr += sprintf(ptr, "%d %s", seconds, repr);
    }
    ptr += sprintf(ptr, " remaining)               ");
    return (buf);
}


# define FACTORIAL(n) (n * (n + 1) / 2)
static int      get_remaining_time(int base_time, int cur_time)
{
    double      chunk_time;
    int         missing_chunks;
    int         remaining;

    chunk_time = (double)(cur_time - base_time) / (double)g_vars.treated_chunks;
    missing_chunks = FACTORIAL(g_vars.num_chunks) - g_vars.treated_chunks;
    if (missing_chunks == 0)
    {
        remaining = 0;
    }
    else
    {
        remaining = (int)(chunk_time * (double)missing_chunks);
        if (remaining == 0)
        {
            remaining = -1;
        }
    }
    return (remaining);
}


void            print_remaining_time(void)
{
    static int  base_time = 0;
    int         cur_time;
    static int  prev_remaining = 0;
    int         remaining;

    if (base_time == 0)
    {
        base_time = time(NULL);
        return;
    }
    cur_time = time(NULL);
    if (cur_time == base_time)
    {
        return;
    }
    remaining = get_remaining_time(base_time, cur_time);
    if (remaining == 0)
    {
        if (prev_remaining != 0)
        {
            printf("\rRemoving duplicates ... %s\n", time_repr(0));
            fflush(stdout);
        }
        prev_remaining = 0;
    }
    else if (remaining < 0)
    {
        return;
    }
    else if (prev_remaining == 0 || remaining < prev_remaining)
    {
        printf("\rRemoving duplicates ... %s\r", time_repr(remaining));
        fflush(stdout);
        prev_remaining = remaining;
    }
    else if (prev_remaining)
    {
        printf("\rRemoving duplicates ... %s\r", time_repr(prev_remaining));
        fflush(stdout);
    }
}
