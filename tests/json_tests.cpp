#include <cassert>
#include <string>
import quilibrium.core;
import quilibrium.json;
int main(){
    auto parsed=quilibrium::json::parse(R"({"user":{"fid":3,"username":"dwr.eth"},"ok":true})");
    assert(parsed);
    assert(parsed->bool_or("ok"));
    auto* user=parsed->find("user"); assert(user); assert(user->uint64_or("fid")==3); assert(user->string_or("username")=="dwr.eth");
    assert(quilibrium::json::parse(quilibrium::json::stringify(*parsed)));
    return 0;
}
