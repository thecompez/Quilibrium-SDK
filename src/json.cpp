module;
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

module quilibrium.json;

namespace quilibrium::json {
namespace {

class parser final {
public:
    explicit parser(std::string_view input) : input_(input) {}

    result<value> run() {
        skip_ws();
        auto parsed = parse_value();
        if (!parsed) return parsed;
        skip_ws();
        if (position_ != input_.size()) return fail("unexpected trailing JSON data");
        return parsed;
    }

private:
    result<value> fail(std::string message) const {
        return std::unexpected(error{.domain=error_domain::protocol,.code=700,.message=std::move(message) + " at byte " + std::to_string(position_)});
    }

    void skip_ws() {
        while (position_ < input_.size()) {
            const char c = input_[position_];
            if (c != ' ' && c != '\n' && c != '\r' && c != '\t') break;
            ++position_;
        }
    }

    result<value> parse_value() {
        skip_ws();
        if (position_ >= input_.size()) return fail("unexpected end of JSON");
        switch (input_[position_]) {
            case 'n': return parse_literal("null", value{nullptr});
            case 't': return parse_literal("true", value{true});
            case 'f': return parse_literal("false", value{false});
            case '"': {
                auto text = parse_string();
                if (!text) return std::unexpected(text.error());
                return value{std::move(*text)};
            }
            case '[': return parse_array();
            case '{': return parse_object();
            default:
                if (input_[position_] == '-' || (input_[position_] >= '0' && input_[position_] <= '9')) return parse_number();
                return fail("invalid JSON token");
        }
    }

    result<value> parse_literal(std::string_view literal, value output) {
        if (input_.substr(position_, literal.size()) != literal) return fail("invalid JSON literal");
        position_ += literal.size();
        return output;
    }

    result<std::string> parse_string() {
        if (position_ >= input_.size() || input_[position_] != '"') {
            return std::unexpected(error{.domain=error_domain::protocol,.code=701,.message="expected JSON string"});
        }
        ++position_;
        std::string out;
        while (position_ < input_.size()) {
            const char c = input_[position_++];
            if (c == '"') return out;
            if (static_cast<unsigned char>(c) < 0x20U) {
                return std::unexpected(error{.domain=error_domain::protocol,.code=702,.message="control character in JSON string"});
            }
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (position_ >= input_.size()) return std::unexpected(error{.domain=error_domain::protocol,.code=703,.message="truncated JSON escape"});
            const char esc = input_[position_++];
            switch (esc) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    if (position_ + 4 > input_.size()) return std::unexpected(error{.domain=error_domain::protocol,.code=704,.message="truncated unicode escape"});
                    std::uint32_t cp = 0;
                    for (int i = 0; i < 4; ++i) {
                        const char h = input_[position_++];
                        cp <<= 4U;
                        if (h >= '0' && h <= '9') cp |= static_cast<std::uint32_t>(h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= static_cast<std::uint32_t>(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= static_cast<std::uint32_t>(h - 'A' + 10);
                        else return std::unexpected(error{.domain=error_domain::protocol,.code=705,.message="invalid unicode escape"});
                    }
                    if (cp <= 0x7FU) out.push_back(static_cast<char>(cp));
                    else if (cp <= 0x7FFU) {
                        out.push_back(static_cast<char>(0xC0U | (cp >> 6U)));
                        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
                    } else {
                        out.push_back(static_cast<char>(0xE0U | (cp >> 12U)));
                        out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
                        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
                    }
                    break;
                }
                default: return std::unexpected(error{.domain=error_domain::protocol,.code=706,.message="invalid JSON escape"});
            }
        }
        return std::unexpected(error{.domain=error_domain::protocol,.code=707,.message="unterminated JSON string"});
    }

    result<value> parse_number() {
        const std::size_t start = position_;
        if (input_[position_] == '-') ++position_;
        while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
        if (position_ < input_.size() && input_[position_] == '.') {
            ++position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
        }
        if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_;
            if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) ++position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
        }
        double number = 0.0;
        const auto token = input_.substr(start, position_ - start);
        const auto* begin = token.data();
        const auto* end = begin + token.size();
        const auto parsed = std::from_chars(begin, end, number);
        if (parsed.ec != std::errc{} || parsed.ptr != end || !std::isfinite(number)) return fail("invalid JSON number");
        return value{number};
    }

    result<value> parse_array() {
        ++position_;
        array out;
        skip_ws();
        if (position_ < input_.size() && input_[position_] == ']') { ++position_; return value{std::move(out)}; }
        while (true) {
            auto item = parse_value();
            if (!item) return item;
            out.push_back(std::move(*item));
            skip_ws();
            if (position_ >= input_.size()) return fail("unterminated JSON array");
            if (input_[position_] == ']') { ++position_; break; }
            if (input_[position_] != ',') return fail("expected comma in JSON array");
            ++position_;
        }
        return value{std::move(out)};
    }

    result<value> parse_object() {
        ++position_;
        object out;
        skip_ws();
        if (position_ < input_.size() && input_[position_] == '}') { ++position_; return value{std::move(out)}; }
        while (true) {
            skip_ws();
            auto key = parse_string();
            if (!key) return std::unexpected(key.error());
            skip_ws();
            if (position_ >= input_.size() || input_[position_] != ':') return fail("expected colon in JSON object");
            ++position_;
            auto item = parse_value();
            if (!item) return item;
            out.insert_or_assign(std::move(*key), std::move(*item));
            skip_ws();
            if (position_ >= input_.size()) return fail("unterminated JSON object");
            if (input_[position_] == '}') { ++position_; break; }
            if (input_[position_] != ',') return fail("expected comma in JSON object");
            ++position_;
        }
        return value{std::move(out)};
    }

    std::string_view input_{};
    std::size_t position_{};
};

void append_escaped(std::string& out, std::string_view text) {
    out.push_back('"');
    for (const char ch : text) {
        const auto c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20U) {
                    static constexpr char digits[] = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(digits[(c >> 4U) & 0xFU]);
                    out.push_back(digits[c & 0xFU]);
                } else out.push_back(static_cast<char>(c));
        }
    }
    out.push_back('"');
}

void append_value(std::string& out, const value& input) {
    if (std::holds_alternative<std::nullptr_t>(input.data)) { out += "null"; return; }
    if (const auto* v = std::get_if<bool>(&input.data)) { out += *v ? "true" : "false"; return; }
    if (const auto* v = std::get_if<double>(&input.data)) {
        std::ostringstream stream;
        stream << std::setprecision(17) << *v;
        out += stream.str();
        return;
    }
    if (const auto* v = std::get_if<std::string>(&input.data)) { append_escaped(out, *v); return; }
    if (const auto* v = std::get_if<array>(&input.data)) {
        out.push_back('[');
        bool first = true;
        for (const auto& item : *v) { if (!first) out.push_back(','); first = false; append_value(out, item); }
        out.push_back(']');
        return;
    }
    const auto& obj = std::get<object>(input.data);
    out.push_back('{');
    bool first = true;
    for (const auto& [key, item] : obj) {
        if (!first) out.push_back(',');
        first = false;
        append_escaped(out, key); out.push_back(':'); append_value(out, item);
    }
    out.push_back('}');
}

} // namespace

value::value() : data(nullptr) {}
value::value(std::nullptr_t) : data(nullptr) {}
value::value(bool v) : data(v) {}
value::value(double v) : data(v) {}
value::value(std::string v) : data(std::move(v)) {}
value::value(array v) : data(std::move(v)) {}
value::value(object v) : data(std::move(v)) {}

bool value::is_null() const noexcept { return std::holds_alternative<std::nullptr_t>(data); }
const object* value::as_object() const noexcept { return std::get_if<object>(&data); }
const array* value::as_array() const noexcept { return std::get_if<array>(&data); }
const std::string* value::as_string() const noexcept { return std::get_if<std::string>(&data); }
std::optional<double> value::as_number() const noexcept { if (const auto* v=std::get_if<double>(&data)) return *v; return std::nullopt; }
std::optional<bool> value::as_bool() const noexcept { if (const auto* v=std::get_if<bool>(&data)) return *v; return std::nullopt; }
const value* value::find(std::string_view key) const noexcept {
    const auto* obj = as_object(); if (!obj) return nullptr;
    const auto it = obj->find(key); return it == obj->end() ? nullptr : &it->second;
}
std::string value::string_or(std::string_view key, std::string fallback) const {
    if (const auto* item=find(key)) if (const auto* text=item->as_string()) return *text;
    return fallback;
}
std::uint64_t value::uint64_or(std::string_view key, std::uint64_t fallback) const noexcept {
    if (const auto* item=find(key)) if (const auto number=item->as_number(); number && *number >= 0.0 && *number <= static_cast<double>(std::numeric_limits<std::uint64_t>::max())) return static_cast<std::uint64_t>(*number);
    return fallback;
}
bool value::bool_or(std::string_view key, bool fallback) const noexcept {
    if (const auto* item=find(key)) if (const auto v=item->as_bool()) return *v;
    return fallback;
}

result<value> parse(std::string_view text) { return parser{text}.run(); }
std::string stringify(const value& input) { std::string out; append_value(out, input); return out; }

} // namespace quilibrium::json
