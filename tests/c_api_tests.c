#include "quilibrium.h"
#include <assert.h>
#include <string.h>

int main(void) {
    assert(strcmp(ql_version(),"1.1.0")==0);
    ql_error error={0};
    ql_sdk* sdk=ql_sdk_create(NULL,&error);
    assert(sdk!=NULL);
    assert(error.message==NULL);
    ql_sdk_destroy(sdk);
    ql_error_free(&error);
    return 0;
}
