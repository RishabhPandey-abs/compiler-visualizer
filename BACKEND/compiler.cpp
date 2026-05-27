#include <iostream>
#include <vector>
#include <string>
#include <regex>
#include <set>
#include <map>
#include <algorithm>

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

vector<string> parserSteps;
vector<string> treeSteps;

vector<string> intermediateCode;
vector<string> optimizedCode;
vector<string> finalCode;

bool hasError = false;
string errorMessage = "";

int tempCount = 1;
int labelCount = 1;

// ---------------- HELPERS ----------------
string newTemp() {
    return "t" + to_string(tempCount++);
}

string newLabel() {
    return "L" + to_string(labelCount++);
}

void syntaxError(string msg) {
    hasError = true;
    errorMessage = msg;
}

// ---------------- KEYWORDS ----------------
set<string> keywords = {
    "auto","break","case","char","const","continue","default","do","double","else",
    "enum","extern","float","for","goto","if","int","long","register","return",
    "short","signed","sizeof","static","struct","switch","typedef","union",
    "unsigned","void","volatile","while"
};

bool isKeyword(string s) {
    return keywords.count(s);
}

bool isIdentifier(string s) {
    return regex_match(s, regex(R"([a-zA-Z_][a-zA-Z0-9_]*)")) && !isKeyword(s);
}

bool isNumber(string s) {
    return regex_match(s, regex(R"([0-9]+)"));
}

// ---------------- TOKENIZER ----------------
vector<string> tokenize(string code) {

    vector<string> result;

    regex pattern(
        R"([a-zA-Z_][a-zA-Z0-9_]*|[0-9]+|==|!=|<=|>=|<|>|[=+\-*/(){}]|;)"
    );

    auto begin = sregex_iterator(code.begin(), code.end(), pattern);
    auto end = sregex_iterator();

    for (auto i = begin; i != end; ++i)
        result.push_back(i->str());

    return result;
}

// ---------------- TYPE ----------------
string getType(string lex) {

    if (isKeyword(lex))
        return "Keyword";

    if (isIdentifier(lex))
        return "Identifier";

    if (isNumber(lex))
        return "Number";

    if (regex_match(lex, regex(R"([+\-*/=<>!]+)")))
        return "Operator";

    return "Delimiter";
}

// ---------------- LEXICAL ----------------
vector<Token> lexicalAnalysis(string code) {

    vector<Token> result;

    for (auto &l : tokenize(code)) {
        result.push_back({l, getType(l)});
    }

    return result;
}

// ---------------- PARSER ----------------
Token current() {

    if (pos < tokens.size())
        return tokens[pos];

    return {"",""};
}

void advance() {

    if (pos < tokens.size()) {

        parserSteps.push_back(
            "Shift: " + tokens[pos].lexeme
        );

        pos++;
    }
}

// ---------------- TREE STRING ----------------
void buildTreeString(
    Node* root,
    string prefix,
    bool isLast,
    string &out
) {

    if (!root) return;

    out += prefix;
    out += (isLast ? "└── " : "├── ");
    out += root->value + "\n";

    for (int i = 0; i < root->children.size(); i++) {

        buildTreeString(
            root->children[i],
            prefix + (isLast ? "    " : "│   "),
            i == root->children.size() - 1,
            out
        );
    }
}

void saveTreeStep(Node* root) {

    string tree = "";

    buildTreeString(root, "", true, tree);

    treeSteps.push_back(tree);
}

// ---------------- EXPRESSIONS ----------------
Node* parseExpression();

Node* parseFactor() {

    if (hasError) return NULL;

    Token t = current();

    // Parenthesis
    if (t.lexeme == "(") {

        advance();

        Node* node = parseExpression();

        if (current().lexeme == ")") {
            advance();
        }
        else {
            syntaxError("Missing closing ')' ");
            return NULL;
        }

        return node;
    }

    // Identifier / Number
    if (t.type == "Identifier" || t.type == "Number") {

        parserSteps.push_back(
            "Reduce: Factor -> " + t.lexeme
        );

        advance();

        return new Node(t.lexeme);
    }

    syntaxError(
        "Invalid expression near: " + t.lexeme
    );

    return NULL;
}

Node* parseTerm() {

    Node* node = parseFactor();

    if (hasError || node == NULL)
        return NULL;

    while (
        current().lexeme == "*" ||
        current().lexeme == "/"
    ) {

        string op = current().lexeme;

        parserSteps.push_back(
            "Reduce: Term -> Term " + op + " Factor"
        );

        advance();

        Node* right = parseFactor();

        if (right == NULL)
            return NULL;

        Node* newNode = new Node(op);

        newNode->children.push_back(node);
        newNode->children.push_back(right);

        parserSteps.push_back(
            "Create Node: " + op
        );

        node = newNode;

        saveTreeStep(node);
    }

    return node;
}

Node* parseExpression() {

    Node* node = parseTerm();

    if (hasError || node == NULL)
        return NULL;

    while (
        current().lexeme == "+" ||
        current().lexeme == "-"
    ) {

        string op = current().lexeme;

        parserSteps.push_back(
            "Reduce: Expr -> Expr " + op + " Term"
        );

        advance();

        Node* right = parseTerm();

        if (right == NULL)
            return NULL;

        Node* newNode = new Node(op);

        newNode->children.push_back(node);
        newNode->children.push_back(right);

        parserSteps.push_back(
            "Create Node: " + op
        );

        node = newNode;

        saveTreeStep(node);
    }

    return node;
}

// ---------------- CONDITION ----------------
Node* parseCondition() {

    Node* left = parseExpression();

    if (left == NULL)
        return NULL;

    if (
        current().lexeme == "<" ||
        current().lexeme == ">" ||
        current().lexeme == "==" ||
        current().lexeme == "!="
    ) {

        string op = current().lexeme;

        advance();

        Node* right = parseExpression();

        if (right == NULL)
            return NULL;

        Node* node = new Node(op);

        node->children.push_back(left);
        node->children.push_back(right);

        saveTreeStep(node);

        return node;
    }

    return left;
}

// ---------------- STATEMENT ----------------
Node* parseStatement() {

    if (hasError)
        return NULL;

    // ---------------- IF ----------------
    if (current().lexeme == "if") {

        parserSteps.push_back("Parsing IF");

        advance();

        if (current().lexeme != "(") {
            syntaxError("Expected '(' after if");
            return NULL;
        }

        advance();

        Node* cond = parseCondition();

        if (cond == NULL)
            return NULL;

        if (current().lexeme == ")") {
            advance();
        }
        else {
            syntaxError("Missing ')' after condition");
            return NULL;
        }

        Node* ifBlock = new Node("BLOCK");

        if (current().lexeme == "{") {

            advance();

            while (
                current().lexeme != "}" &&
                pos < tokens.size()
            ) {

                Node* stmt = parseStatement();

                if (stmt == NULL)
                    return NULL;

                ifBlock->children.push_back(stmt);
            }

            if (current().lexeme == "}") {
                advance();
            }
            else {
                syntaxError("Missing '}' ");
                return NULL;
            }
        }
        else {
            syntaxError("Expected '{' after if condition");
            return NULL;
        }

        Node* root = new Node("IF");

        root->children.push_back(cond);
        root->children.push_back(ifBlock);

        // ---------------- ELSE SUPPORT ----------------
        if(current().lexeme == "else"){

            advance();

            Node* elseBlock = new Node("ELSE");

            if(current().lexeme == "{"){

                advance();

                while(
                    current().lexeme != "}" &&
                    pos < tokens.size()
                ){

                    Node* stmt = parseStatement();

                    if(stmt == NULL)
                        return NULL;

                    elseBlock->children.push_back(stmt);
                }

                if(current().lexeme == "}"){
                    advance();
                }
                else{
                    syntaxError("Missing '}' after else block");
                    return NULL;
                }
            }
            else{
                syntaxError("Expected '{' after else");
                return NULL;
            }

            root->children.push_back(elseBlock);
        }

        saveTreeStep(root);

        return root;
    }

    // ---------------- DECLARATION ----------------
    if (current().type == "Keyword") {

        parserSteps.push_back(
            "Parsing Declaration"
        );

        advance();
    }

    // ---------------- IDENTIFIER ----------------
    if (current().type != "Identifier") {

        syntaxError(
            "Expected identifier near: " +
            current().lexeme
        );

        return NULL;
    }

    string var = current().lexeme;

    parserSteps.push_back(
        "Identifier: " + var
    );

    advance();

    // ---------------- = ----------------
    if (current().lexeme != "=") {

        syntaxError(
            "Expected '=' after identifier"
        );

        return NULL;
    }

    advance();

    // ---------------- EXPRESSION ----------------
    Node* expr = parseExpression();

    if (expr == NULL) {

        syntaxError(
            "Invalid expression"
        );

        return NULL;
    }

    // ---------------- ; ----------------
    if (current().lexeme == ";") {

        advance();
    }
    else {

        syntaxError(
            "Missing semicolon ';'"
        );

        return NULL;
    }

    Node* root = new Node("=");

    root->children.push_back(
        new Node(var)
    );

    root->children.push_back(expr);

    parserSteps.push_back(
        "Create Assignment Node (=)"
    );

    saveTreeStep(root);

    return root;
}

// ---------------- PROGRAM ----------------
Node* parseProgram() {

    Node* program = new Node("PROGRAM");

    while (
        pos < tokens.size() &&
        !hasError
    ) {

        Node* stmt = parseStatement();

        if (stmt == NULL)
            return NULL;

        program->children.push_back(stmt);
    }

    saveTreeStep(program);

    return program;
}

// ---------------- IC ----------------
string generateIC(Node* root) {

    if (!root)
        return "";

    if (root->children.empty())
        return root->value;

    // PROGRAM / BLOCK / ELSE
    if (
        root->value == "PROGRAM" ||
        root->value == "BLOCK" ||
        root->value == "ELSE"
    ) {

        for (auto c : root->children)
            generateIC(c);

        return "";
    }

    // ASSIGNMENT
    if (root->value == "=") {

        string rhs =
            generateIC(root->children[1]);

        intermediateCode.push_back(
            root->children[0]->value +
            " = " + rhs
        );

        return root->children[0]->value;
    }

    // IF ELSE
    if (root->value == "IF") {

        string cond =
            generateIC(root->children[0]);

        string falseLabel = newLabel();
        string endLabel = newLabel();

        intermediateCode.push_back(
            "ifFalse " + cond +
            " goto " + falseLabel
        );

        // IF BLOCK
        generateIC(root->children[1]);

        // ELSE EXISTS
        if(root->children.size() == 3){

            intermediateCode.push_back(
                "goto " + endLabel
            );

            intermediateCode.push_back(
                falseLabel + ":"
            );

            generateIC(root->children[2]);

            intermediateCode.push_back(
                endLabel + ":"
            );
        }
        else{

            intermediateCode.push_back(
                falseLabel + ":"
            );
        }

        return "";
    }

    // OPERATIONS
    string left =
        generateIC(root->children[0]);

    string right =
        generateIC(root->children[1]);

    string temp = newTemp();

    intermediateCode.push_back(
        temp + " = " +
        left + " " +
        root->value + " " +
        right
    );

    return temp;
}

// ---------------- OPTIMIZE CODE ----------------
void optimizeCode() {

    optimizedCode.clear();

    map<string, string> values;

    for (auto &line : intermediateCode) {

        smatch match;

        regex assignPattern(
            R"((\w+)\s*=\s*(\w+))"
        );

        regex exprPattern(
            R"((\w+)\s*=\s*(\w+)\s*([\+\-\*/])\s*(\w+))"
        );

        if (regex_match(line, match, exprPattern)) {

            string lhs = match[1];
            string op1 = match[2];
            string op  = match[3];
            string op2 = match[4];

            if (values.count(op1))
                op1 = values[op1];

            if (values.count(op2))
                op2 = values[op2];

            if (
                regex_match(op1, regex(R"(\d+)")) &&
                regex_match(op2, regex(R"(\d+)"))
            ) {

                int a = stoi(op1);
                int b = stoi(op2);

                int result = 0;

                if (op == "+") result = a + b;
                else if (op == "-") result = a - b;
                else if (op == "*") result = a * b;
                else if (op == "/" && b != 0)
                    result = a / b;

                values[lhs] =
                    to_string(result);

                optimizedCode.push_back(
                    lhs + " = " +
                    to_string(result)
                );
            }
            else {

                optimizedCode.push_back(
                    lhs + " = " +
                    op1 + " " +
                    op + " " +
                    op2
                );
            }
        }

        else if (
            regex_match(line, match, assignPattern)
        ) {

            string lhs = match[1];
            string rhs = match[2];

            if (values.count(rhs))
                rhs = values[rhs];

            values[lhs] = rhs;

            if (lhs != rhs) {

                optimizedCode.push_back(
                    lhs + " = " + rhs
                );
            }
        }

        else {

            optimizedCode.push_back(line);
        }
    }

    set<string> usedVariables;

    vector<string> finalOptimized;

    for (
        int i = optimizedCode.size() - 1;
        i >= 0;
        i--
    ) {

        string line = optimizedCode[i];

        smatch match;

        regex assignPattern(
            R"((\w+)\s*=\s*(.*))"
        );

        if (
            regex_match(line, match, assignPattern)
        ) {

            string lhs = match[1];
            string rhs = match[2];

            if (
                usedVariables.count(lhs) ||
                lhs[0] != 't'
            ) {

                finalOptimized.push_back(line);

                regex varPattern(
                    R"([a-zA-Z_][a-zA-Z0-9_]*)"
                );

                auto begin =
                    sregex_iterator(
                        rhs.begin(),
                        rhs.end(),
                        varPattern
                    );

                auto end = sregex_iterator();

                for (
                    auto j = begin;
                    j != end;
                    ++j
                ) {

                    string var = j->str();

                    if (
                        !regex_match(
                            var,
                            regex(R"(\d+)")
                        )
                    ) {
                        usedVariables.insert(var);
                    }
                }
            }
        }
        else {

            finalOptimized.push_back(line);
        }
    }

    reverse(
        finalOptimized.begin(),
        finalOptimized.end()
    );

    optimizedCode = finalOptimized;
}

// ---------------- FINAL ----------------
void generateFinalCode() {

    for (auto &l : optimizedCode) {

        finalCode.push_back(
            "MOV " + l
        );
    }
}

// ---------------- MAIN ----------------
void runCompiler(string code) {

    hasError = false;
    errorMessage = "";

    tokens = lexicalAnalysis(code);

    pos = 0;

    parserSteps.clear();
    treeSteps.clear();

    intermediateCode.clear();
    optimizedCode.clear();
    finalCode.clear();

    tempCount = 1;
    labelCount = 1;

    // ---------------- TOKENS ----------------
    cout << "=== TOKENS ===\n";

    for (auto &t : tokens) {

        cout << t.lexeme
             << " -> "
             << t.type
             << endl;
    }

    // ---------------- PARSE ----------------
    Node* tree = parseProgram();

    // ---------------- ERROR ----------------
    if (hasError || tree == NULL) {

        cout << "\n=== ERRORS ===\n";

        cout << errorMessage << endl;

        return;
    }

    // ---------------- PARSE TREE STEPS ----------------
    cout << "\n=== PARSE TREE STEPS ===\n";

    int step = 1;

    for (auto &t : treeSteps) {

        cout << "STEP "
             << step++
             << ":\n";

        cout << t << "\n";
    }

    // ---------------- FINAL PARSE TREE ----------------
    string finalTree = "";

    buildTreeString(
        tree,
        "",
        true,
        finalTree
    );

    cout << "\n=== FINAL PARSE TREE ===\n";

    cout << finalTree;

    // ---------------- INTERMEDIATE CODE ----------------
    generateIC(tree);

    cout << "\n=== INTERMEDIATE CODE ===\n";

    for (auto &l : intermediateCode)
        cout << l << endl;

    // ---------------- OPTIMIZATION ----------------
    optimizeCode();

    cout << "\n=== OPTIMIZED CODE ===\n";

    for (auto &l : optimizedCode)
        cout << l << endl;

    // ---------------- FINAL CODE ----------------
    generateFinalCode();

    cout << "\n=== FINAL CODE ===\n";

    for (auto &l : finalCode)
        cout << l << endl;
}

int main() {

    string code, line;

    while (getline(cin, line)) {

        code += line + "\n";
    }

    runCompiler(code);

    return 0;
}

