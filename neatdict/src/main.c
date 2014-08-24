#include <stdio.h>
#include "definitions.h"

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    DLOG("test");
    DLOG("improved %s", "test");
    return (42);
}
