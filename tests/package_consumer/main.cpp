#include <cstdlib>

import quilibrium;

int main()
{
    const auto sdk = quilibrium::connect();
    return sdk ? EXIT_SUCCESS : EXIT_FAILURE;
}
