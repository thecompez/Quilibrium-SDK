module;
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

export module quilibrium.json;
import quilibrium.core;

export namespace quilibrium::json {

struct value;
using array = std::vector<value>;
using object = std::map<std::string, value, std::less<>>;

struct value final {
    using storage = std::variant<std::nullptr_t, bool, double, std::string, array, object>;
    storage data{nullptr};

    value();
    value(std::nullptr_t);
    value(bool v);
    value(double v);
    value(std::string v);
    value(array v);
    value(object v);

    [[nodiscard]] bool is_null() const noexcept;
    [[nodiscard]] const object* as_object() const noexcept;
    [[nodiscard]] const array* as_array() const noexcept;
    [[nodiscard]] const std::string* as_string() const noexcept;
    [[nodiscard]] std::optional<double> as_number() const noexcept;
    [[nodiscard]] std::optional<bool> as_bool() const noexcept;
    [[nodiscard]] const value* find(std::string_view key) const noexcept;
    [[nodiscard]] std::string string_or(std::string_view key, std::string fallback = {}) const;
    [[nodiscard]] std::uint64_t uint64_or(std::string_view key, std::uint64_t fallback = 0) const noexcept;
    [[nodiscard]] bool bool_or(std::string_view key, bool fallback = false) const noexcept;
};

/** Parses a strict JSON document. */
[[nodiscard]] result<value> parse(std::string_view text);

/** Serializes a value to compact JSON. */
[[nodiscard]] std::string stringify(const value& input);

} // namespace quilibrium::json
