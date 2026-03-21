#include <iostream>
#include <vector>
#include <string>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <regex>

using namespace std;

// ---------------- TOKEN STRUCT ----------------
struct Token {
    string lexeme;
    string type;
};

// ---------------- CHECK FUNCTIONS ----------------
bool isKeyword(const string& s) {
    static const vector<string> keywords = {
        "auto", "break", "case", "char", "const", "continue",
        "default", "do", "double", "else", "enum", "extern",
        "float", "for", "goto", "if", "int", "long",
        "register", "return", "short", "signed", "sizeof",
        "static", "struct", "switch", "typedef", "union",
        "unsigned", "void", "volatile", "while"
    };

    for (const auto& kw : keywords) {
        if (s == kw) return true;
    }
    return false;
}

bool isIdentifier(const string& s) {
    return regex_match(s, regex(R"([a-zA-Z_][a-zA-Z0-9_]*)"));
}

bool isNumber(const string& s) {
    return regex_match(s, regex(R"([0-9]+)"));
}

// ---------------- TOKENIZER (FIXED) ----------------
vector<string> tokenize(const string& code) {
    vector<string> tokens;

    regex pattern(
        R"(%\{|%\}|%%|#include|<[^>]+>|"[^"]*"|//.*|[a-zA-Z_][a-zA-Z0-9_]*|\[[^\]]+\]|\.\*|[0-9]+|==|!=|<=|>=|[=+\-*/]|[(){};])"
    );

    auto begin = sregex_iterator(code.begin(), code.end(), pattern);
    auto end = sregex_iterator();

    for (auto i = begin; i != end; ++i) {
        tokens.push_back(i->str());
    }

    return tokens;
}

// ---------------- TYPE DETECTION ----------------
string getType(const string& lex) {

    if (isKeyword(lex)) return "Keyword";

    if (lex == "#include") return "Preprocessor";

    if (regex_match(lex, regex(R"(<[^>]+>)"))) return "Header";

    if (regex_match(lex, regex(R"(".*")"))) return "String";

    if (regex_match(lex, regex(R"(//.*)"))) return "Comment";

    if (lex == "%{" || lex == "%}" || lex == "%%") return "Lex Symbol";

    if (regex_match(lex, regex(R"(\[[^\]]+\])")) || lex == ".*")
        return "Regex Pattern";

    if (isIdentifier(lex)) return "Identifier";

    if (isNumber(lex)) return "Number";

    if (lex == "=" || lex == "+" || lex == "-" || lex == "*" || lex == "/")
        return "Operator";

    if (lex == ";" || lex == "(" || lex == ")" || lex == "{" || lex == "}")
        return "Delimiter";

    return "Invalid";
}

// ---------------- LEXICAL ANALYSIS ----------------
vector<Token> lexicalAnalysis(const string& code) {
    vector<string> lexemes = tokenize(code);
    vector<Token> result;

    for (auto &lex : lexemes) {
        Token t;
        t.lexeme = lex;
        t.type = getType(lex);
        result.push_back(t);
    }

    return result;
}

// ---------------- MAIN COMPILER ----------------
void runCompiler(string code) {

    auto tokens = lexicalAnalysis(code);

    // -------- LEXEMES --------
    cout << "=== LEXEMES ===\n";
    for (auto &t : tokens) {
        cout << t.lexeme << "\n";
    }

    // -------- TOKENS --------
    cout << "\n=== TOKENS ===\n";
    cout << "LEXEME TOKEN_TYPE\n";
    cout << "----------------------\n";

    for (auto &t : tokens) {
        cout << t.lexeme << " " << t.type << "\n";
    }

    // -------- SYNTAX --------
    cout << "\n=== SYNTAX ANALYSIS ===\n";
    if (!tokens.empty() && tokens.back().lexeme == ";")
        cout << "✔ Valid statement\n";
    else
        cout << "✘ Missing semicolon\n";

    // -------- PARSE TREE --------
    cout << "\n=== PARSE TREE ===\n";
    cout << "Statement\n";
    cout << "  |\n";
    for (auto &t : tokens) {
        cout << "  --> " << t.lexeme << " (" << t.type << ")\n";
    }

    // -------- SEMANTIC --------
    cout << "\n=== SEMANTIC ANALYSIS ===\n";
    bool hasAssign = false;

    for (auto &t : tokens) {
        if (t.lexeme == "=") hasAssign = true;
    }

    if (hasAssign)
        cout << "✔ Assignment detected\n";
    else
        cout << "✘ No assignment found\n";
}

// ---------------- MAIN ----------------
int main() {
    string code, line;

    while (getline(cin, line)) {
        code += line + "\n";
    }

    runCompiler(code);
    return 0;
}
