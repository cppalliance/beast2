//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include <boost/http_io/server/router.hpp>

#include <map>
#include <functional>
#include <regex>
#include <string>
#include <stdexcept>
#include <string>
#include <vector>
#include "test_suite.hpp"

namespace boost {
namespace http_io {

// Exception for path parsing errors (inherits from std::runtime_error)
class PathError : public std::runtime_error {
public:
    std::string originalPath;
    PathError(const std::string& message, const std::string& path)
        : std::runtime_error([&]() {
              std::string text = message;
              if (!path.empty()) {
                  text += ": " + path;
              }
              return text;
          }()),
          originalPath(path) {}
};

// Enum for the different token types in a parsed path pattern
enum class TokenType { Text, Param, Wildcard, Group };

// Structure representing a token in the parsed pattern
struct Token {
    TokenType type;
    std::string value;              // used for literal text (Text tokens)
    std::string name;               // used for parameters (Param/Wildcard tokens)
    std::vector<Token> tokens;      // used for Group tokens (contains nested tokens)
    Token(TokenType t) : type(t) {}
    Token(TokenType t, const std::string& text) : type(t) {
        if (t == TokenType::Text) {
            value = text;
        } else if (t == TokenType::Param || t == TokenType::Wildcard) {
            name = text;
        }
    }
    Token(TokenType t, std::vector<Token>&& groupTokens) 
        : type(t), tokens(std::move(groupTokens)) {}
};

// Escape regex special characters in a string (e.g., ".", "$", "*" etc.)
static std::string escapeRegex(const std::string& str) {
    std::string result;
    result.reserve(str.size() * 2);
    for (char c : str) {
        // Characters with special meaning in regex that need escaping
        if (c == '.' || c == '+' || c == '*' || c == '?' || c == '^' || c == '$' ||
            c == '{' || c == '}' || c == '(' || c == ')' || c == '[' || c == ']' ||
            c == '|' || c == '/' || c == '\\') {
            result.push_back('\\');  // prepend backslash
        }
        result.push_back(c);
    }
    return result;
}

// Parse a path pattern string into a vector of Tokens (may throw PathError on invalid syntax)
static std::vector<Token> parse(const std::string& pattern) {
    // Define an internal LexToken type for the first-pass tokens
    enum class LexType {
        LBrace, RBrace, LParen, RParen, LBracket, RBracket,  // group or reserved symbols
        Plus, QMark, Excl,    // other special symbols (currently not used in final output)
        Escape, Char, Param, Wildcard, End
    };
    struct LexToken { LexType type; size_t index; std::string value; 
        LexToken(LexType t, size_t i, const std::string& v) : type(t), index(i), value(v) {} };
    
    std::vector<LexToken> lexTokens;
    lexTokens.reserve(pattern.size() + 1);
    size_t index = 0;

    // Helper: parse a parameter name (after ':' or '*'), handling quotes and escapes
    auto parseName = [&](void) -> std::string {
        std::string name;
        if (index < pattern.size()) {
            char c = pattern[index];
            // Allowed start: letter (A-Z,a-z), underscore, or dollar sign
            if ((c == '_') || (c == '$') ||
                ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) {
                // Consume all valid identifier chars (letters, digits, '_' or '$')
                do {
                    name.push_back(pattern[index++]);
                    if (index >= pattern.size()) break;
                    c = pattern[index];
                } while ((c == '_') || (c == '$') ||
                         ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')));
            } else if (c == '"') {
                // Quoted parameter name, allows special characters
                size_t quoteStart = index;
                index++;  // skip the opening quote
                while (index < pattern.size()) {
                    char ch = pattern[index++];
                    if (ch == '"') {
                        // Closing quote found
                        quoteStart = 0;
                        break;
                    }
                    if (ch == '\\' && index < pattern.size()) {
                        // Escape sequence: take next char literally
                        name.push_back(pattern[index++]);
                    } else {
                        name.push_back(ch);
                    }
                }
                if (quoteStart != 0) {
                    // If quoteStart is still set, no closing quote was found
                    throw PathError("Unterminated quote at index " + std::to_string(quoteStart), pattern);
                }
            }
        }
        if (name.empty()) {
            throw PathError("Missing parameter name at index " + std::to_string(index), pattern);
        }
        return name;
    };

    // 1. Lexical analysis: create lexTokens from input pattern characters
    while (index < pattern.size()) {
        char c = pattern[index];
        switch (c) {
            case '{':
                lexTokens.emplace_back(LexType::LBrace, index, "{");
                index++;
                break;
            case '}':
                lexTokens.emplace_back(LexType::RBrace, index, "}");
                index++;
                break;
            case '(':
                lexTokens.emplace_back(LexType::LParen, index, "(");
                index++;
                break;
            case ')':
                lexTokens.emplace_back(LexType::RParen, index, ")");
                index++;
                break;
            case '[':
                lexTokens.emplace_back(LexType::LBracket, index, "[");
                index++;
                break;
            case ']':
                lexTokens.emplace_back(LexType::RBracket, index, "]");
                index++;
                break;
            case '+':
                lexTokens.emplace_back(LexType::Plus, index, "+");
                index++;
                break;
            case '?':
                lexTokens.emplace_back(LexType::QMark, index, "?");
                index++;
                break;
            case '!':
                lexTokens.emplace_back(LexType::Excl, index, "!");
                index++;
                break;
            case '\\':  // Escape character
                if (index + 1 >= pattern.size()) {
                    throw PathError("Trailing escape (\\) at end of pattern", pattern);
                }
                // Take the next character as a literal
                index++;
                lexTokens.emplace_back(LexType::Escape, index - 1, std::string(1, pattern[index]));
                index++;
                break;
            case ':': {  // Start of a named parameter
                index++;
                std::string paramName = parseName();
                // Record a Param token (value holds the parameter name)
                lexTokens.emplace_back(LexType::Param, index - paramName.size() - 1, paramName);
                break;
            }
            case '*': {  // Start of a wildcard parameter
                index++;
                std::string wildcardName = parseName();
                lexTokens.emplace_back(LexType::Wildcard, index - wildcardName.size() - 1, wildcardName);
                break;
            }
            default:
                // Regular character (no special meaning in pattern syntax)
                lexTokens.emplace_back(LexType::Char, index, std::string(1, c));
                index++;
                break;
        }
    }
    // Append an End token to mark the end of input
    lexTokens.emplace_back(LexType::End, index, "");

    // 2. Structural analysis: convert lexTokens to hierarchical Tokens
    size_t pos = 0;
    // Recursive lambda to consume tokens until a given end type (or end of sequence)
    std::function<std::vector<Token>(LexType)> consumeUntil = [&](LexType endType) -> std::vector<Token> {
        std::vector<Token> output;
        while (true) {
            LexToken token = lexTokens[pos++];
            if (token.type == endType) {
                break;  // Reached the end of the current group or pattern
            }
            if (token.type == LexType::Char || token.type == LexType::Escape) {
                // Merge consecutive literal characters (Char/Escape) into one Text token
                std::string literal = token.value;
                // Continue to accumulate subsequent chars or escapes
                while (pos < lexTokens.size()) {
                    LexToken next = lexTokens[pos];
                    if (next.type == LexType::Char || next.type == LexType::Escape) {
                        literal += next.value;
                        pos++;
                        continue;
                    }
                    break;
                }
                output.emplace_back(TokenType::Text, literal);
                continue;
            }
            if (token.type == LexType::Param) {
                // Named parameter token
                output.emplace_back(TokenType::Param, token.value);
                continue;
            }
            if (token.type == LexType::Wildcard) {
                // Wildcard parameter token
                output.emplace_back(TokenType::Wildcard, token.value);
                continue;
            }
            if (token.type == LexType::LBrace) {
                // Start of an optional group: recursively parse until the matching `}`
                std::vector<Token> groupTokens = consumeUntil(LexType::RBrace);
                output.emplace_back(TokenType::Group, std::move(groupTokens));
                continue;
            }
            // If we encounter any other token types in an unexpected context, it's an error.
            // This catches cases like unmatched braces or reserved characters ((), [], +, ?, !) in invalid places.
            std::string errChar = token.value.empty() ? "token" : ("'" + token.value + "'");
            std::string expected = (endType == LexType::RBrace ? "}'" : "end of pattern");
            throw PathError("Unexpected " + errChar + " at index " + std::to_string(token.index) +
                            ", expected " + expected, pattern);
        }
        return output;
    };

    std::vector<Token> tokens = consumeUntil(LexType::End);
    return tokens;
}

// Flatten tokens by expanding optional groups into separate sequences of tokens
static void flattenTokens(const std::vector<Token>& tokens,
                          size_t index,
                          std::vector<Token>& currentSequence,
                          std::vector< std::vector<Token> >& sequences) {
    if (index == tokens.size()) {
        // Reached end of one possible sequence
        sequences.push_back(currentSequence);
        return;
    }
    const Token& token = tokens[index];
    if (token.type == TokenType::Group) {
        // An optional group: branch into two cases – include it or skip it.
        // Case 1: Include the group tokens. We flatten the group's content first.
        std::vector< std::vector<Token> > groupExpansions;
        groupExpansions.reserve(2);
        flattenTokens(token.tokens, 0, currentSequence, groupExpansions);
        // For each expansion of the group content, continue flattening the rest of the pattern
        for (auto& seq : groupExpansions) {
            flattenTokens(tokens, index + 1, seq, sequences);
        }
        // Case 2: Omit the group entirely. Continue flattening from the next token after the group.
        flattenTokens(tokens, index + 1, currentSequence, sequences);
    } else {
        // Non-group token: append it to the current sequence and move to the next token
        currentSequence.push_back(token);
        flattenTokens(tokens, index + 1, currentSequence, sequences);
        // Backtrack: remove the token to restore state for other branches
        currentSequence.pop_back();
    }
}

// Generate a regex pattern fragment from a flat token sequence and collect parameter keys
static std::string toRegExpSource(const std::vector<Token>& tokens,
                                  char delimiter,
                                  std::vector<std::pair<std::string, TokenType>>& keys,
                                  const std::string& originalPath) {
    std::string result;
    result.reserve(tokens.size() * 5);  // preallocate some space for efficiency
    std::string backtrack;
    bool isSafeSegmentParam = true;
    for (const Token& token : tokens) {
        if (token.type == TokenType::Text) {
            // Append literal text to the pattern (escape special regex chars)
            result += escapeRegex(token.value);
            backtrack += token.value;
            // If the text contains the delimiter, it means we've hit a segment boundary.
            if (token.value.find(delimiter) != std::string::npos) {
                isSafeSegmentParam = true;
            }
            // If delimiter not found in this text, keep isSafeSegmentParam as is.
            continue;
        }
        if (token.type == TokenType::Param || token.type == TokenType::Wildcard) {
            // If a parameter appears immediately after another parameter or at segment start 
            // without preceding text, ensure there's no unsafe segment.
            if (!isSafeSegmentParam && backtrack.empty()) {
                throw PathError(std::string("Missing text before \"") + token.name + "\"" +
                                (token.type == TokenType::Param ? " param" : " wildcard"),
                                originalPath);
            }
            if (token.type == TokenType::Param) {
                // Standard parameter: match any chars except the delimiter (at least one char)
                result += "(";
                // Determine a pattern to avoid backtracking conflicts with the previous text.
                // If the parameter sits in the same segment as prior text, we disallow certain 
                // characters to prevent overlapping with that text.
                auto makeNegPattern = [&](const std::string& bt) {
                    // If backtrack (bt) is short (<2 chars) or empty, or delimiter is 1 char,
                    // use simple patterns, otherwise use lookahead for longer strings.
                    if (bt.size() < 2) {
                        // If backtrack is empty or single char, exclude delimiter + backtrack char
                        return std::string("[^") + escapeRegex(std::string(1, delimiter) + bt) + "]";
                    } else {
                        // Backtrack has length >= 2, use a negative lookahead to block that sequence
                        return std::string("(?:(?!") + escapeRegex(bt) + ")" +
                               "[^" + escapeRegex(std::string(1, delimiter)) + "])";
                    }
                };
                // Use an empty backtrack for a fresh segment, or the actual backtrack text if in same segment
                std::string negClass = makeNegPattern(isSafeSegmentParam ? "" : backtrack);
                result += negClass + "+)";  // one or more of the allowed characters
            } else {
                // Wildcard parameter: match one or more of *any* character (including delimiter)
                result += "([\\s\\S]+)";
            }
            // Record the parameter key (name and type) for this capturing group
            keys.emplace_back(token.name, token.type);
            // Reset segment context after a parameter
            backtrack.clear();
            isSafeSegmentParam = false;
            continue;
        }
        // Group tokens should not appear here, as sequences are flattened (no TokenType::Group in `tokens`).
    }
    return result;
}

// Structure to hold the result of pathToRegexp: the regex and the parameter keys
struct PathToRegexpResult {
    std::regex regex;
    std::vector< std::pair<std::string, TokenType> > keys;
};

// Main function: convert a path pattern string to a regex (and keys list)
PathToRegexpResult pathToRegexp(const std::string& path,
                                bool end = true,
                                bool trailing = true,
                                bool sensitive = false,
                                char delimiter = '/') {
    // Parse the path pattern into tokens (throws PathError on invalid patterns)
    std::vector<Token> tokens = parse(path);

    // Flatten optional groups into a list of flat token sequences
    std::vector< std::vector<Token> > sequences;
    std::vector<Token> current;
    flattenTokens(tokens, 0, current, sequences);

    // Build regex sources for each sequence and collect keys
    std::vector< std::pair<std::string, TokenType> > keys;
    std::vector<std::string> sources;
    sources.reserve(sequences.size());
    for (const auto& seq : sequences) {
        std::string sourcePattern = toRegExpSource(seq, delimiter, keys, path);
        sources.push_back(sourcePattern);
    }

    // Join all sequence patterns into one overall pattern (alternate with '|')
    std::string pattern = "^(?:";  // begin regex, non-capturing group for alternation
    if (!sources.empty()) {
        pattern += sources[0];
        for (size_t i = 1; i < sources.size(); ++i) {
            pattern += "|" + sources[i];
        }
    }
    pattern += ")";

    // Handle the trailing delimiter option: if true, allow an optional delimiter at the end
    if (trailing) {
        pattern += "(?:" + escapeRegex(std::string(1, delimiter)) + "$)?";
    }
    // If `end` is true, anchor the regex to match the full string; 
    // if false, allow partial matches (but ensure the next character is a delimiter or EOS).
    if (end) {
        pattern += "$";
    } else {
        pattern += "(?=" + escapeRegex(std::string(1, delimiter)) + "|$)";
    }

    // Compile the regex with the desired case sensitivity
    std::regex::flag_type flags = std::regex::ECMAScript;
    if (!sensitive) {
        flags |= std::regex::icase;
    }
    std::regex compiled(pattern, flags);

    // Prepare the result
    PathToRegexpResult result;
    result.regex = std::move(compiled);
    result.keys = std::move(keys);
    return result;
}
// Assume pathToRegexp and its dependent code are defined as provided earlier.

struct Request {
    std::string method;
    std::string path;
    std::map<std::string, std::string> params;
};

struct Response {
    int status = 200;
    std::string body;
    void send(const std::string& text) { body = text; }
    void json(const std::string& jsonText) { body = jsonText; }
};

class Router {
public:
    using Handler = std::function<void(Request&, Response&)>;

    void get(const std::string& path, Handler handler) {
        addRoute("GET", path, std::move(handler));
    }

    void post(const std::string& path, Handler handler) {
        addRoute("POST", path, std::move(handler));
    }

    void put(const std::string& path, Handler handler) {
        addRoute("PUT", path, std::move(handler));
    }

    void del(const std::string& path, Handler handler) {
        addRoute("DELETE", path, std::move(handler));
    }

    void all(const std::string& path, Handler handler) {
        addRoute("ALL", path, std::move(handler));
    }

    bool route(Request& req, Response& res) const {
        for (const auto& route : routes_) {
            if (route.method != "ALL" && route.method != req.method) continue;
            std::smatch match;
            if (std::regex_match(req.path, match, route.regex.regex)) {
                // Extract parameters based on keys
                size_t captureIndex = 1;
                for (const auto& key : route.regex.keys) {
                    if (captureIndex < match.size()) {
                        req.params[key.first] = match[captureIndex++];
                    }
                }
                route.handler(req, res);
                return true;
            }
        }
        res.status = 404;
        res.body = "Not Found";
        return false;
    }

private:
    struct Route {
        std::string method;
        PathToRegexpResult regex;
        Handler handler;
    };
    std::vector<Route> routes_;

    void addRoute(const std::string& method, const std::string& path, Handler handler) {
        Route route;
        route.method = method;
        route.regex = pathToRegexp(path);
        route.handler = std::move(handler);
        routes_.push_back(std::move(route));
    }
};

#if 0
// Example usage of the simple router:
int main() {
    Router router;

    router.get("/users/:id", [](Request& req, Response& res) {
        res.send("User ID: " + req.params["id"]);
    });

    router.post("/users", [](Request& req, Response& res) {
        res.send("Create new user");
    });

    router.get("/about", [](Request& req, Response& res) {
        res.send("About page");
    });

    // Simulated request dispatch
    std::vector<Request> testRequests = {
        {"GET", "/users/123", {}},
        {"POST", "/users", {}},
        {"GET", "/about", {}},
        {"GET", "/unknown", {}}
    };

    for (auto& req : testRequests) {
        Response res;
        router.route(req, res);
        std::cout << req.method << " " << req.path << " -> "
                  << res.status << " " << res.body << std::endl;
    }

    return 0;
}
#endif

struct router_test
{
    void
    run()
    {
        {
            auto result = pathToRegexp("/dir/:id");
        }
        {
            auto result = pathToRegexp("/dir/:id");
        }
    }
};

TEST_SUITE(
    router_test,
    "boost.http_io.server.router");

} // http_io
} // boost
