#include <stdlib.h>

#include <quilibrium.h>

int main(void)
{
    return ql_version() != NULL ? EXIT_SUCCESS : EXIT_FAILURE;
}
