#include <cassert>
#include <cmath>
#include <string>

import quilibrium.core;
import quilibrium.json;

int main()
{
    const auto parsed = quilibrium::json::parse(
        R"({"user":{"fid":3,"username":"dwr.eth"},"ratio":12.5,"exponent":1.25e2,"ok":true})"
    );

    assert(parsed);
    assert(parsed->bool_or("ok"));

    const auto* user = parsed->find("user");
    assert(user != nullptr);
    assert(user->uint64_or("fid") == 3);
    assert(user->string_or("username") == "dwr.eth");

    const auto* ratio = parsed->find("ratio");
    assert(ratio != nullptr);
    assert(ratio->as_number().has_value());
    assert(std::abs(*ratio->as_number() - 12.5) < 1e-12);

    const auto* exponent = parsed->find("exponent");
    assert(exponent != nullptr);
    assert(exponent->as_number().has_value());
    assert(std::abs(*exponent->as_number() - 125.0) < 1e-12);

    const auto serialized = quilibrium::json::stringify(*parsed);
    assert(quilibrium::json::parse(serialized));

    return 0;
}
