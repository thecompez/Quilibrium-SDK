#include <cassert>
#include <string>
import quilibrium.core;
int main(){
    using namespace quilibrium;
    const std::string sample="Q"; assert(hex(as_bytes(sample))=="51");
    auto decoded=unhex("51"); assert(decoded&&decoded->size()==1);
    auto e=parse_endpoint("https://example.com:8443/api"); assert(e); assert(e->host=="example.com"); assert(e->port==8443); assert(e->base_path=="/api");
    assert(percent_encode("a b/c") == "a%20b%2Fc");
    endpoint def{.scheme="https",.host="example.com",.port=443}; assert(def.origin()=="https://example.com");
    return 0;
}
