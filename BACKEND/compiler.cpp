#include <iostream>
#include <vector>
#include <string>
#include <regex>
#include <map>
#include <set>

using namespace std;

// ---------------- TOKEN ----------------
struct Token {
    string lexeme;
    string type;
};

// ---------------- TREE ----------------
struct Node {
    string value;
    vector<Node*> children;

    Node(string v) : value(v) {}
};

// ---------------- GLOBAL ----------------
vector<Token> tokens;
int pos = 0;

vector<string> intermediateCode;
vector<string> optimizedCode;
vector<string> finalCode;

int tempCount = 1;
int labelCount = 1;

// ---------------- HELPERS ----------------
string newTemp() { return "t" + to_string(tempCount++); }
string newLabel() { return "L" + to_string(labelCount++); }

// ---------------- KEYWORDS (32) ----------------
set<string> keywords = {
    "auto","break","case","char","const","continue","default","do","double","else",
    "enum","extern","float","for","goto","if","int","long","register","return",
    "short","signed","sizeof","static","struct","switch","typedef","union",
    "unsigned","void","volatile","while"
};

bool isKeyword(string s) { return keywords.count(s); }

bool isIdentifier(string s) {
    return regex_match(s, regex(R"([a-zA-Z_][a-zA-Z0-9_]*)")) && !isKeyword(s);
}

bool isNumber(string s) {
    return regex_match(s, regex(R"([0-9]+)"));
}

// ---------------- TOKENIZER ----------------
vector<string> tokenize(string code) {
    vector<string> result;
    regex pattern(R"([a-zA-Z_][a-zA-Z0-9_]*|[0-9]+|==|!=|<=|>=|<|>|[=+\-*/(){}]|;)");
    auto begin = sregex_iterator(code.begin(), code.end(), pattern);
    auto end = sregex_iterator();

    for (auto i = begin; i != end; ++i)
        result.push_back(i->str());

    return result;
}

// ---------------- TYPE ----------------
string getType(string lex) {
    if (isKeyword(lex)) return "Keyword";
    if (isIdentifier(lex)) return "Identifier";
    if (isNumber(lex)) return "Number";
    if (regex_match(lex, regex(R"([+\-*/=<>!]+)"))) return "Operator";
    if (lex == "(" || lex == ")" || lex == "{" || lex == "}" || lex == ";")
        return "Delimiter";
    return "Invalid";
}

// ---------------- LEXICAL ----------------
vector<Token> lexicalAnalysis(string code) {
    vector<Token> result;
    for (auto &l : tokenize(code))
        result.push_back({l, getType(l)});
    return result;
}

// ---------------- PARSER ----------------
Token current() {
    if (pos < tokens.size()) return tokens[pos];
    return {"",""};
}

void advance() { if (pos < tokens.size()) pos++; }

// ---------------- EXPRESSIONS ----------------
Node* parseExpression();

Node* parseFactor() {
    Token t = current();

    if (t.lexeme == "(") {
        advance();
        Node* node = parseExpression();
        if (current().lexeme == ")") advance();
        return node;
    }

    if (t.type == "Identifier" || t.type == "Number") {
        advance();
        return new Node(t.lexeme);
    }

    return NULL;
}

Node* parseTerm() {
    Node* node = parseFactor();

    while (current().lexeme == "*" || current().lexeme == "/") {
        string op = current().lexeme;
        advance();
        Node* right = parseFactor();

        Node* newNode = new Node(op);
        newNode->children.push_back(node);
        newNode->children.push_back(right);
        node = newNode;
    }

    return node;
}

Node* parseExpression() {
    Node* node = parseTerm();

    while (current().lexeme == "+" || current().lexeme == "-") {
        string op = current().lexeme;
        advance();
        Node* right = parseTerm();

        Node* newNode = new Node(op);
        newNode->children.push_back(node);
        newNode->children.push_back(right);
        node = newNode;
    }

    return node;
}

Node* parseCondition() {
    Node* left = parseExpression();

    if (current().lexeme == "<" || current().lexeme == ">" ||
        current().lexeme == "==" || current().lexeme == "!=") {

        string op = current().lexeme;
        advance();
        Node* right = parseExpression();

        Node* node = new Node(op);
        node->children.push_back(left);
        node->children.push_back(right);
        return node;
    }

    return left;
}

// ---------------- STATEMENT ----------------
Node* parseStatement() {

    // IF
    if (current().lexeme == "if") {
        advance(); // if
        advance(); // (

        Node* cond = parseCondition();
        if (current().lexeme == ")") advance();

        Node* thenStmt = nullptr;
        Node* elseStmt = nullptr;

        // THEN BLOCK
        if (current().lexeme == "{") {
            advance();
            Node* block = new Node("BLOCK");

            while (current().lexeme != "}" && pos < tokens.size())
                block->children.push_back(parseStatement());

            if (current().lexeme == "}") advance();
            thenStmt = block;
        } else {
            thenStmt = parseStatement();
        }

        // ELSE
        if (current().lexeme == "else") {
            advance();

            if (current().lexeme == "{") {
                advance();
                Node* block = new Node("BLOCK");

                while (current().lexeme != "}" && pos < tokens.size())
                    block->children.push_back(parseStatement());

                if (current().lexeme == "}") advance();
                elseStmt = block;
            } else {
                elseStmt = parseStatement();
            }
        }

        Node* root = new Node("IF");
        root->children.push_back(cond);
        root->children.push_back(thenStmt);

        if (elseStmt)
            root->children.push_back(elseStmt);

        return root;
    }

    // declaration or assignment
    if (current().type == "Keyword") advance();

    string var = current().lexeme;
    advance(); // id
    advance(); // =

    Node* expr = parseExpression();

    if (current().lexeme == ";") advance();

    Node* root = new Node("=");
    root->children.push_back(new Node(var));
    root->children.push_back(expr);

    return root;
}

// ---------------- PROGRAM ----------------
Node* parseProgram() {
    Node* program = new Node("PROGRAM");

    while (pos < tokens.size()) {
        Node* stmt = parseStatement();
        if (!stmt) break;
        program->children.push_back(stmt);
    }

    return program;
}

// ---------------- TREE PRINT ----------------
void printTree(Node* root, int depth = 0) {
    if (!root) return;

    for (int i = 0; i < depth; i++) cout << "  ";
    cout << root->value << endl;

    for (auto child : root->children)
        printTree(child, depth + 1);
}

// ---------------- INTERMEDIATE CODE ----------------
string generateIC(Node* root) {
    if (!root) return "";

    // PROGRAM
    if (root->value == "PROGRAM") {
        for (auto child : root->children)
            generateIC(child);
        return "";
    }

    // BLOCK
    if (root->value == "BLOCK") {
        for (auto child : root->children)
            generateIC(child);
        return "";
    }

    if (root->children.empty())
        return root->value;

    // IF
    if (root->value == "IF") {
        string cond = generateIC(root->children[0]);

        string L1 = newLabel();
        string L2 = newLabel();

        intermediateCode.push_back("ifFalse " + cond + " goto " + L1);

        generateIC(root->children[1]);
        intermediateCode.push_back("goto " + L2);

        intermediateCode.push_back(L1 + ":");

        if (root->children.size() > 2)
            generateIC(root->children[2]);

        intermediateCode.push_back(L2 + ":");

        return "";
    }

    // ASSIGNMENT
    if (root->value == "=") {
        string rhs = generateIC(root->children[1]);
        intermediateCode.push_back(root->children[0]->value + " = " + rhs);
        return root->children[0]->value;
    }

    // BINARY OP
    if (root->children.size() < 2)
        return root->value;

    string left = generateIC(root->children[0]);
    string right = generateIC(root->children[1]);

    string temp = newTemp();
    intermediateCode.push_back(temp + " = " + left + " " + root->value + " " + right);

    return temp;
}

// ---------------- OPTIMIZER ----------------
void optimizeCode() {
    vector<string> optimized;
    map<string, string> constMap;

    for (string line : intermediateCode) {

        smatch m;
        regex exprConst(R"((t\d+) = ([0-9]+) ([+\-*/]) ([0-9]+))");

        if (regex_match(line, m, exprConst)) {
            int a = stoi(m[2]);
            int b = stoi(m[4]);
            char op = m[3].str()[0];

            int result = (op == '+') ? a + b :
                         (op == '-') ? a - b :
                         (op == '*') ? a * b :
                         (b != 0 ? a / b : 0);

            optimized.push_back(m[1].str() + " = " + to_string(result));
            constMap[m[1]] = to_string(result);
        }
        else {
            for (auto &p : constMap)
                line = regex_replace(line, regex("\\b" + p.first + "\\b"), p.second);

            optimized.push_back(line);
        }
    }

    optimizedCode = optimized;
}

// ---------------- FINAL CODE ----------------
void generateFinalCode() {
    for (auto &line : optimizedCode)
        finalCode.push_back("MOV " + line);
}

// ---------------- MAIN ----------------
void runCompiler(string code) {

    tokens = lexicalAnalysis(code);
    pos = 0;

    cout << "=== TOKENS ===\n";
    for (auto &t : tokens)
        cout << t.lexeme << " -> " << t.type << endl;

    Node* tree = parseProgram();

    cout << "\n=== FINAL PARSE TREE ===\n";
    printTree(tree);

    generateIC(tree);

    cout << "\n=== INTERMEDIATE CODE ===\n";
    for (auto &l : intermediateCode)
        cout << l << endl;

    optimizeCode();

    cout << "\n=== OPTIMIZED CODE ===\n";
    for (auto &l : optimizedCode)
        cout << l << endl;

    generateFinalCode();

    cout << "\n=== FINAL CODE ===\n";
    for (auto &l : finalCode)
        cout << l << endl;
}

int main() {
    string code, line;
    while (getline(cin, line))
        code += line + "\n";

    runCompiler(code);
}