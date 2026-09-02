module;
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iomanip>
#include <ios>
#include <limits>
#include <locale>
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

[[nodiscard]] bool parse_floating_point(std::string_view token, double& number)
{
#if defined(QUILIBRIUM_PLATFORM_APPLE)
    // libc++ currently marks floating-point std::from_chars as unavailable for
    // deployment targets older than macOS 26. Use the classic C locale through
    // a stream on Apple platforms so JSON parsing remains compatible with older
    // supported macOS releases while preserving locale-independent '.' syntax.
    std::istringstream stream{std::string{token}};
    stream.imbue(std::locale::classic());
    stream >> std::noskipws >> number;

    if (stream.fail() || !std::isfinite(number)) {
        return false;
    }

    return stream.peek() == std::char_traits<char>::eof();
#else
    const auto* begin = token.data();
    const auto* end = begin + token.size();
    const auto parsed = std::from_chars(begin, end, number);

    return parsed.ec == std::errc{} && parsed.ptr == end && std::isfinite(number);
#endif
}

class parser final {
public:
    explicit parser(std::string_view input)
        : input_(input)
    {
    }

    result<value> run()
    {
        skip_ws();
        auto parsed = parse_value();
        if (!parsed) {
            return parsed;
        }

        skip_ws();
        if (position_ != input_.size()) {
            return fail("unexpected trailing JSON data");
        }

        return parsed;
    }

private:
    result<value> fail(std::string message) const
    {
        return std::unexpected(error{
            .domain = error_domain::protocol,
            .code = 700,
            .message = std::move(message) + " at byte " + std::to_string(position_)
        });
    }

    void skip_ws()
    {
        while (position_ < input_.size()) {
            const char character = input_[position_];
            if (character != ' ' && character != '\n' && character != '\r' && character != '\t') {
                break;
            }
            ++position_;
        }
    }

    result<value> parse_value()
    {
        skip_ws();
        if (position_ >= input_.size()) {
            return fail("unexpected end of JSON");
        }

        switch (input_[position_]) {
            case 'n':
                return parse_literal("null", value{nullptr});
            case 't':
                return parse_literal("true", value{true});
            case 'f':
                return parse_literal("false", value{false});
            case '"': {
                auto text = parse_string();
                if (!text) {
                    return std::unexpected(text.error());
                }
                return value{std::move(*text)};
            }
            case '[':
                return parse_array();
            case '{':
                return parse_object();
            default:
                if (input_[position_] == '-' ||
                    (input_[position_] >= '0' && input_[position_] <= '9')) {
                    return parse_number();
                }
                return fail("invalid JSON token");
        }
    }

    result<value> parse_literal(std::string_view literal, value output)
    {
        if (input_.substr(position_, literal.size()) != literal) {
            return fail("invalid JSON literal");
        }

        position_ += literal.size();
        return output;
    }

    result<std::string> parse_string()
    {
        if (position_ >= input_.size() || input_[position_] != '"') {
            return std::unexpected(error{
                .domain = error_domain::protocol,
                .code = 701,
                .message = "expected JSON string"
            });
        }

        ++position_;
        std::string output;

        while (position_ < input_.size()) {
            const char character = input_[position_++];
            if (character == '"') {
                return output;
            }

            if (static_cast<unsigned char>(character) < 0x20U) {
                return std::unexpected(error{
                    .domain = error_domain::protocol,
                    .code = 702,
                    .message = "control character in JSON string"
                });
            }

            if (character != '\\') {
                output.push_back(character);
                continue;
            }

            if (position_ >= input_.size()) {
                return std::unexpected(error{
                    .domain = error_domain::protocol,
                    .code = 703,
                    .message = "truncated JSON escape"
                });
            }

            const char escape = input_[position_++];
            switch (escape) {
                case '"':
                    output.push_back('"');
                    break;
                case '\\':
                    output.push_back('\\');
                    break;
                case '/':
                    output.push_back('/');
                    break;
                case 'b':
                    output.push_back('\b');
                    break;
                case 'f':
                    output.push_back('\f');
                    break;
                case 'n':
                    output.push_back('\n');
                    break;
                case 'r':
                    output.push_back('\r');
                    break;
                case 't':
                    output.push_back('\t');
                    break;
                case 'u': {
                    if (position_ + 4 > input_.size()) {
                        return std::unexpected(error{
                            .domain = error_domain::protocol,
                            .code = 704,
                            .message = "truncated unicode escape"
                        });
                    }

                    std::uint32_t codePoint = 0;
                    for (int index = 0; index < 4; ++index) {
                        const char hex = input_[position_++];
                        codePoint <<= 4U;

                        if (hex >= '0' && hex <= '9') {
                            codePoint |= static_cast<std::uint32_t>(hex - '0');
                        } else if (hex >= 'a' && hex <= 'f') {
                            codePoint |= static_cast<std::uint32_t>(hex - 'a' + 10);
                        } else if (hex >= 'A' && hex <= 'F') {
                            codePoint |= static_cast<std::uint32_t>(hex - 'A' + 10);
                        } else {
                            return std::unexpected(error{
                                .domain = error_domain::protocol,
                                .code = 705,
                                .message = "invalid unicode escape"
                            });
                        }
                    }

                    if (codePoint <= 0x7FU) {
                        output.push_back(static_cast<char>(codePoint));
                    } else if (codePoint <= 0x7FFU) {
                        output.push_back(static_cast<char>(0xC0U | (codePoint >> 6U)));
                        output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
                    } else {
                        output.push_back(static_cast<char>(0xE0U | (codePoint >> 12U)));
                        output.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
                        output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
                    }
                    break;
                }
                default:
                    return std::unexpected(error{
                        .domain = error_domain::protocol,
                        .code = 706,
                        .message = "invalid JSON escape"
                    });
            }
        }

        return std::unexpected(error{
            .domain = error_domain::protocol,
            .code = 707,
            .message = "unterminated JSON string"
        });
    }

    result<value> parse_number()
    {
        const std::size_t start = position_;

        if (input_[position_] == '-') {
            ++position_;
        }

        while (position_ < input_.size() &&
               input_[position_] >= '0' && input_[position_] <= '9') {
            ++position_;
        }

        if (position_ < input_.size() && input_[position_] == '.') {
            ++position_;
            while (position_ < input_.size() &&
                   input_[position_] >= '0' && input_[position_] <= '9') {
                ++position_;
            }
        }

        if (position_ < input_.size() &&
            (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_;
            if (position_ < input_.size() &&
                (input_[position_] == '+' || input_[position_] == '-')) {
                ++position_;
            }

            while (position_ < input_.size() &&
                   input_[position_] >= '0' && input_[position_] <= '9') {
                ++position_;
            }
        }

        double number = 0.0;
        const auto token = input_.substr(start, position_ - start);
        if (!parse_floating_point(token, number)) {
            return fail("invalid JSON number");
        }

        return value{number};
    }

    result<value> parse_array()
    {
        ++position_;
        array output;
        skip_ws();

        if (position_ < input_.size() && input_[position_] == ']') {
            ++position_;
            return value{std::move(output)};
        }

        while (true) {
            auto item = parse_value();
            if (!item) {
                return item;
            }

            output.push_back(std::move(*item));
            skip_ws();

            if (position_ >= input_.size()) {
                return fail("unterminated JSON array");
            }

            if (input_[position_] == ']') {
                ++position_;
                break;
            }

            if (input_[position_] != ',') {
                return fail("expected comma in JSON array");
            }

            ++position_;
        }

        return value{std::move(output)};
    }

    result<value> parse_object()
    {
        ++position_;
        object output;
        skip_ws();

        if (position_ < input_.size() && input_[position_] == '}') {
            ++position_;
            return value{std::move(output)};
        }

        while (true) {
            skip_ws();
            auto key = parse_string();
            if (!key) {
                return std::unexpected(key.error());
            }

            skip_ws();
            if (position_ >= input_.size() || input_[position_] != ':') {
                return fail("expected colon in JSON object");
            }

            ++position_;
            auto item = parse_value();
            if (!item) {
                return item;
            }

            output.insert_or_assign(std::move(*key), std::move(*item));
            skip_ws();

            if (position_ >= input_.size()) {
                return fail("unterminated JSON object");
            }

            if (input_[position_] == '}') {
                ++position_;
                break;
            }

            if (input_[position_] != ',') {
                return fail("expected comma in JSON object");
            }

            ++position_;
        }

        return value{std::move(output)};
    }

    std::string_view input_{};
    std::size_t position_{};
};

void append_escaped(std::string& output, std::string_view text)
{
    output.push_back('"');

    for (const char character : text) {
        const auto value = static_cast<unsigned char>(character);
        switch (value) {
            case '"':
                output += "\\\"";
                break;
            case '\\':
                output += "\\\\";
                break;
            case '\b':
                output += "\\b";
                break;
            case '\f':
                output += "\\f";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            default:
                if (value < 0x20U) {
                    static constexpr char digits[] = "0123456789abcdef";
                    output += "\\u00";
                    output.push_back(digits[(value >> 4U) & 0xFU]);
                    output.push_back(digits[value & 0xFU]);
                } else {
                    output.push_back(static_cast<char>(value));
                }
        }
    }

    output.push_back('"');
}

void append_value(std::string& output, const value& input)
{
    if (std::holds_alternative<std::nullptr_t>(input.data)) {
        output += "null";
        return;
    }

    if (const auto* boolean = std::get_if<bool>(&input.data)) {
        output += *boolean ? "true" : "false";
        return;
    }

    if (const auto* number = std::get_if<double>(&input.data)) {
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << std::setprecision(17) << *number;
        output += stream.str();
        return;
    }

    if (const auto* text = std::get_if<std::string>(&input.data)) {
        append_escaped(output, *text);
        return;
    }

    if (const auto* values = std::get_if<array>(&input.data)) {
        output.push_back('[');
        bool isFirst = true;

        for (const auto& item : *values) {
            if (!isFirst) {
                output.push_back(',');
            }
            isFirst = false;
            append_value(output, item);
        }

        output.push_back(']');
        return;
    }

    const auto& values = std::get<object>(input.data);
    output.push_back('{');
    bool isFirst = true;

    for (const auto& [key, item] : values) {
        if (!isFirst) {
            output.push_back(',');
        }
        isFirst = false;
        append_escaped(output, key);
        output.push_back(':');
        append_value(output, item);
    }

    output.push_back('}');
}

} // namespace

value::value()
    : data(nullptr)
{
}

value::value(std::nullptr_t)
    : data(nullptr)
{
}

value::value(bool input)
    : data(input)
{
}

value::value(double input)
    : data(input)
{
}

value::value(std::string input)
    : data(std::move(input))
{
}

value::value(array input)
    : data(std::move(input))
{
}

value::value(object input)
    : data(std::move(input))
{
}

bool value::is_null() const noexcept
{
    return std::holds_alternative<std::nullptr_t>(data);
}

const object* value::as_object() const noexcept
{
    return std::get_if<object>(&data);
}

const array* value::as_array() const noexcept
{
    return std::get_if<array>(&data);
}

const std::string* value::as_string() const noexcept
{
    return std::get_if<std::string>(&data);
}

std::optional<double> value::as_number() const noexcept
{
    if (const auto* number = std::get_if<double>(&data)) {
        return *number;
    }
    return std::nullopt;
}

std::optional<bool> value::as_bool() const noexcept
{
    if (const auto* boolean = std::get_if<bool>(&data)) {
        return *boolean;
    }
    return std::nullopt;
}

const value* value::find(std::string_view key) const noexcept
{
    const auto* values = as_object();
    if (values == nullptr) {
        return nullptr;
    }

    const auto iterator = values->find(key);
    return iterator == values->end() ? nullptr : &iterator->second;
}

std::string value::string_or(std::string_view key, std::string fallback) const
{
    if (const auto* item = find(key)) {
        if (const auto* text = item->as_string()) {
            return *text;
        }
    }

    return fallback;
}

std::uint64_t value::uint64_or(std::string_view key, std::uint64_t fallback) const noexcept
{
    if (const auto* item = find(key)) {
        if (const auto number = item->as_number();
            number && *number >= 0.0 &&
            *number <= static_cast<double>(std::numeric_limits<std::uint64_t>::max())) {
            return static_cast<std::uint64_t>(*number);
        }
    }

    return fallback;
}

bool value::bool_or(std::string_view key, bool fallback) const noexcept
{
    if (const auto* item = find(key)) {
        if (const auto boolean = item->as_bool()) {
            return *boolean;
        }
    }

    return fallback;
}

result<value> parse(std::string_view text)
{
    return parser{text}.run();
}

std::string stringify(const value& input)
{
    std::string output;
    append_value(output, input);
    return output;
}

} // namespace quilibrium::json
