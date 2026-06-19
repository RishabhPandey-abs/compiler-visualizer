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
// Updated pattern to also capture ':' and ',' for switch/case support
vector<string> tokenize(string code) {

    vector<string> result;

    regex pattern(
        R"([a-zA-Z_][a-zA-Z0-9_]*|[0-9]+|==|!=|<=|>=|<|>|[=+\-*/(){}:;,])"
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

    if (pos < (int)tokens.size())
        return tokens[pos];

    return {"",""};
}

void advance() {

    if (pos < (int)tokens.size()) {

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

    for (int i = 0; i < (int)root->children.size(); i++) {

        buildTreeString(
            root->children[i],
            prefix + (isLast ? "    " : "│   "),
            i == (int)root->children.size() - 1,
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
        current().lexeme == "<"  ||
        current().lexeme == ">"  ||
        current().lexeme == "==" ||
        current().lexeme == "!=" ||
        current().lexeme == "<=" ||
        current().lexeme == ">="
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

// ---------------- FORWARD DECLARE ----------------
Node* parseStatement();

// ---------------- BLOCK HELPER ----------------
// Parses { stmt* } and returns a BLOCK node
Node* parseBlock() {

    Node* block = new Node("BLOCK");

    if (current().lexeme != "{") {
        syntaxError("Expected '{'");
        return NULL;
    }

    advance();

    while (current().lexeme != "}" && pos < (int)tokens.size()) {

        Node* stmt = parseStatement();

        if (stmt == NULL)
            return NULL;

        block->children.push_back(stmt);
    }

    if (current().lexeme == "}") {
        advance();
    }
    else {
        syntaxError("Missing '}'");
        return NULL;
    }

    return block;
}

// ---------------- STATEMENT ----------------
Node* parseStatement() {

    if (hasError)
        return NULL;

    // ================================================================
    // FOR LOOP
    // Syntax: for ( [type] var = expr ; condition ; var = expr ) { }
    // ================================================================
    if (current().lexeme == "for") {

        parserSteps.push_back("Parsing FOR");

        advance(); // consume 'for'

        if (current().lexeme != "(") {
            syntaxError("Expected '(' after for");
            return NULL;
        }

        advance(); // consume '('

        // ---------- INIT ----------
        Node* initNode = NULL;

        if (current().lexeme != ";") {

            // optional type keyword (int, float, etc.)
            if (current().type == "Keyword") {
                advance();
            }

            if (current().type != "Identifier") {
                syntaxError("Expected identifier in for-init");
                return NULL;
            }

            string initVar = current().lexeme;

            advance();

            if (current().lexeme != "=") {
                syntaxError("Expected '=' in for-init");
                return NULL;
            }

            advance();

            Node* initExpr = parseExpression();

            if (initExpr == NULL)
                return NULL;

            initNode = new Node("=");
            initNode->children.push_back(new Node(initVar));
            initNode->children.push_back(initExpr);
        }

        if (current().lexeme == ";") {
            advance(); // consume first ';'
        }
        else {
            syntaxError("Expected ';' after for-init");
            return NULL;
        }

        // ---------- CONDITION ----------
        Node* condNode = NULL;

        if (current().lexeme != ";") {
            condNode = parseCondition();
            if (condNode == NULL)
                return NULL;
        }

        if (current().lexeme == ";") {
            advance(); // consume second ';'
        }
        else {
            syntaxError("Expected ';' after for-condition");
            return NULL;
        }

        // ---------- UPDATE ----------
        Node* updateNode = NULL;

        if (current().lexeme != ")") {

            if (current().type != "Identifier") {
                syntaxError("Expected identifier in for-update");
                return NULL;
            }

            string updateVar = current().lexeme;

            advance();

            if (current().lexeme != "=") {
                syntaxError("Expected '=' in for-update");
                return NULL;
            }

            advance();

            Node* updateExpr = parseExpression();

            if (updateExpr == NULL)
                return NULL;

            updateNode = new Node("=");
            updateNode->children.push_back(new Node(updateVar));
            updateNode->children.push_back(updateExpr);
        }

        if (current().lexeme == ")") {
            advance(); // consume ')'
        }
        else {
            syntaxError("Expected ')' after for-update");
            return NULL;
        }

        // ---------- BODY ----------
        Node* body = parseBlock();

        if (body == NULL)
            return NULL;

        // Build FOR node:
        // children[0] = INIT assignment (or NO_INIT placeholder)
        // children[1] = CONDITION  (or NO_COND placeholder)
        // children[2] = BODY (BLOCK)
        // children[3] = UPDATE assignment (or NO_UPDATE placeholder)

        Node* forNode = new Node("FOR");

        forNode->children.push_back(initNode   ? initNode   : new Node("NO_INIT"));
        forNode->children.push_back(condNode   ? condNode   : new Node("NO_COND"));
        forNode->children.push_back(body);
        forNode->children.push_back(updateNode ? updateNode : new Node("NO_UPDATE"));

        parserSteps.push_back("Create FOR Node");

        saveTreeStep(forNode);

        return forNode;
    }

    // ================================================================
    // SWITCH / CASE
    // Syntax: switch ( expr ) { case val: stmts break; ... default: stmts }
    // ================================================================
    if (current().lexeme == "switch") {

        parserSteps.push_back("Parsing SWITCH");

        advance(); // consume 'switch'

        if (current().lexeme != "(") {
            syntaxError("Expected '(' after switch");
            return NULL;
        }

        advance();

        Node* switchExpr = parseExpression();

        if (switchExpr == NULL)
            return NULL;

        if (current().lexeme == ")") {
            advance();
        }
        else {
            syntaxError("Expected ')' after switch expression");
            return NULL;
        }

        if (current().lexeme != "{") {
            syntaxError("Expected '{' after switch(...)");
            return NULL;
        }

        advance(); // consume '{'

        // SWITCH node: children[0] = switch expression
        //              children[1..n] = CASE or DEFAULT nodes
        Node* switchNode = new Node("SWITCH");
        switchNode->children.push_back(switchExpr);

        while (
            current().lexeme != "}" &&
            pos < (int)tokens.size() &&
            !hasError
        ) {
            // ---------- CASE ----------
            if (current().lexeme == "case") {

                parserSteps.push_back("Parsing CASE");

                advance(); // consume 'case'

                Node* caseVal = parseExpression();

                if (caseVal == NULL)
                    return NULL;

                if (current().lexeme == ":") {
                    advance(); // consume ':'
                }
                else {
                    syntaxError("Expected ':' after case value");
                    return NULL;
                }

                // CASE node: children[0] = case value
                //            children[1..n] = statements
                Node* caseNode = new Node("CASE");
                caseNode->children.push_back(caseVal);

                while (
                    current().lexeme != "case"    &&
                    current().lexeme != "default" &&
                    current().lexeme != "}"       &&
                    pos < (int)tokens.size()      &&
                    !hasError
                ) {
                    if (current().lexeme == "break") {

                        advance(); // consume 'break'

                        if (current().lexeme == ";")
                            advance(); // consume ';'

                        caseNode->children.push_back(new Node("BREAK"));
                        break;
                    }

                    Node* stmt = parseStatement();

                    if (stmt == NULL)
                        return NULL;

                    caseNode->children.push_back(stmt);
                }

                switchNode->children.push_back(caseNode);
            }

            // ---------- DEFAULT ----------
            else if (current().lexeme == "default") {

                parserSteps.push_back("Parsing DEFAULT");

                advance(); // consume 'default'

                if (current().lexeme == ":") {
                    advance(); // consume ':'
                }
                else {
                    syntaxError("Expected ':' after default");
                    return NULL;
                }

                Node* defaultNode = new Node("DEFAULT");

                while (
                    current().lexeme != "}" &&
                    pos < (int)tokens.size() &&
                    !hasError
                ) {
                    if (current().lexeme == "break") {

                        advance();

                        if (current().lexeme == ";")
                            advance();

                        defaultNode->children.push_back(new Node("BREAK"));
                        break;
                    }

                    Node* stmt = parseStatement();

                    if (stmt == NULL)
                        return NULL;

                    defaultNode->children.push_back(stmt);
                }

                switchNode->children.push_back(defaultNode);
            }

            else {
                syntaxError("Unexpected token inside switch: " + current().lexeme);
                return NULL;
            }
        }

        if (current().lexeme == "}") {
            advance(); // consume '}'
        }
        else {
            syntaxError("Missing '}' at end of switch");
            return NULL;
        }

        parserSteps.push_back("Create SWITCH Node");

        saveTreeStep(switchNode);

        return switchNode;
    }

    // ================================================================
    // IF / ELSE  (original code, preserved exactly)
    // ================================================================
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
                pos < (int)tokens.size()
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
        if (current().lexeme == "else") {

            advance();

            Node* elseBlock = new Node("ELSE");

            if (current().lexeme == "{") {

                advance();

                while (
                    current().lexeme != "}" &&
                    pos < (int)tokens.size()
                ) {

                    Node* stmt = parseStatement();

                    if (stmt == NULL)
                        return NULL;

                    elseBlock->children.push_back(stmt);
                }

                if (current().lexeme == "}") {
                    advance();
                }
                else {
                    syntaxError("Missing '}' after else block");
                    return NULL;
                }
            }
            else {
                syntaxError("Expected '{' after else");
                return NULL;
            }

            root->children.push_back(elseBlock);
        }

        saveTreeStep(root);

        return root;
    }

    // ================================================================
    // DECLARATION / ASSIGNMENT  (original code, preserved exactly)
    // ================================================================

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
        pos < (int)tokens.size() &&
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
        root->value == "BLOCK"   ||
        root->value == "ELSE"
    ) {

        for (auto c : root->children)
            generateIC(c);

        return "";
    }

    // ----------------------------------------------------------------
    // ASSIGNMENT  (original)
    // ----------------------------------------------------------------
    if (root->value == "=") {

        string rhs = generateIC(root->children[1]);

        intermediateCode.push_back(
            root->children[0]->value + " = " + rhs
        );

        return root->children[0]->value;
    }

    // ----------------------------------------------------------------
    // IF / ELSE  (original)
    // ----------------------------------------------------------------
    if (root->value == "IF") {

        string cond = generateIC(root->children[0]);

        string falseLabel = newLabel();
        string endLabel   = newLabel();

        intermediateCode.push_back(
            "ifFalse " + cond + " goto " + falseLabel
        );

        generateIC(root->children[1]); // IF block

        if (root->children.size() == 3) {

            intermediateCode.push_back("goto " + endLabel);
            intermediateCode.push_back(falseLabel + ":");

            generateIC(root->children[2]); // ELSE block

            intermediateCode.push_back(endLabel + ":");
        }
        else {
            intermediateCode.push_back(falseLabel + ":");
        }

        return "";
    }

    // ----------------------------------------------------------------
    // FOR LOOP  (new)
    // children: [0]=INIT, [1]=COND, [2]=BODY, [3]=UPDATE
    //
    // Generated layout:
    //   <init>
    //   L_start:
    //     ifFalse <cond> goto L_end
    //     <body>
    //     <update>
    //     goto L_start
    //   L_end:
    // ----------------------------------------------------------------
    if (root->value == "FOR") {

        Node* initNode   = root->children[0];
        Node* condNode   = root->children[1];
        Node* bodyNode   = root->children[2];
        Node* updateNode = root->children[3];

        // Init
        if (initNode->value != "NO_INIT") {
            generateIC(initNode);
        }

        string startLabel = newLabel();
        string endLabel   = newLabel();

        intermediateCode.push_back(startLabel + ":");

        // Condition
        if (condNode->value != "NO_COND") {

            string cond = generateIC(condNode);

            intermediateCode.push_back(
                "ifFalse " + cond + " goto " + endLabel
            );
        }

        // Body
        generateIC(bodyNode);

        // Update
        if (updateNode->value != "NO_UPDATE") {
            generateIC(updateNode);
        }

        intermediateCode.push_back("goto " + startLabel);
        intermediateCode.push_back(endLabel + ":");

        return "";
    }

    // ----------------------------------------------------------------
    // SWITCH / CASE  (new)
    //
    // children: [0]=switch-expr, [1..n]=CASE or DEFAULT nodes
    //
    // Generated layout (comparison-jump approach):
    //   t = switchVar == caseVal        (for each case)
    //   ifTrue  t  goto L_case_i
    //   goto L_default   (or goto L_end if no default)
    //   L_case_1:
    //     <stmts>
    //     goto L_end      (from break)
    //   L_case_2: ...
    //   L_default:
    //     <stmts>
    //   L_end:
    // ----------------------------------------------------------------
    if (root->value == "SWITCH") {

        // Evaluate switch expression into a temp/variable
        string switchVar = generateIC(root->children[0]);

        string endLabel     = newLabel();
        string defaultLabel = "";

        // Pre-allocate one label per CASE/DEFAULT child
        vector<string> caseLabels;

        for (int i = 1; i < (int)root->children.size(); i++) {
            caseLabels.push_back(newLabel());
        }

        // Emit comparison jumps for each case
        int labelIdx = 0;

        for (int i = 1; i < (int)root->children.size(); i++) {

            Node* child = root->children[i];

            if (child->value == "CASE") {

                string caseVal = generateIC(child->children[0]);
                string temp    = newTemp();

                intermediateCode.push_back(
                    temp + " = " + switchVar + " == " + caseVal
                );

                intermediateCode.push_back(
                    "ifTrue " + temp + " goto " + caseLabels[labelIdx]
                );
            }
            else if (child->value == "DEFAULT") {

                defaultLabel = caseLabels[labelIdx];
            }

            labelIdx++;
        }

        // If there's a default, jump to it; otherwise skip to end
        if (!defaultLabel.empty()) {
            intermediateCode.push_back("goto " + defaultLabel);
        }
        else {
            intermediateCode.push_back("goto " + endLabel);
        }

        // Emit case bodies
        labelIdx = 0;

        for (int i = 1; i < (int)root->children.size(); i++) {

            Node* child = root->children[i];

            intermediateCode.push_back(caseLabels[labelIdx] + ":");

            // Start body from index 1 for CASE (index 0 is the case value)
            int startChild = (child->value == "CASE") ? 1 : 0;

            for (int j = startChild; j < (int)child->children.size(); j++) {

                if (child->children[j]->value == "BREAK") {
                    intermediateCode.push_back("goto " + endLabel);
                }
                else {
                    generateIC(child->children[j]);
                }
            }

            labelIdx++;
        }

        intermediateCode.push_back(endLabel + ":");

        return "";
    }

    // ----------------------------------------------------------------
    // BREAK (standalone — safety fallback, normally handled above)
    // ----------------------------------------------------------------
    if (root->value == "BREAK") {
        return "";
    }

    // ----------------------------------------------------------------
    // ARITHMETIC / RELATIONAL OPERATIONS  (original)
    // ----------------------------------------------------------------
    string left  = generateIC(root->children[0]);
    string right = generateIC(root->children[1]);
    string temp  = newTemp();

    intermediateCode.push_back(
        temp + " = " + left + " " + root->value + " " + right
    );

    return temp;
}

// ---------------- OPTIMIZE CODE ----------------
void optimizeCode() {

    optimizedCode.clear();

    map<string, string> values;

    // Patterns
    regex exprPattern(
        R"((\w+)\s*=\s*(\w+)\s*([\+\-\*/])\s*(\w+))"
    );

    regex cmpPattern(
        R"((\w+)\s*=\s*(\w+)\s*(==|!=|<=|>=|<|>)\s*(\w+))"
    );

    regex assignPattern(
        R"((\w+)\s*=\s*(\w+))"
    );

    for (auto &line : intermediateCode) {

        smatch match;

        if (regex_match(line, match, exprPattern)) {

            string lhs = match[1];
            string op1 = match[2];
            string op  = match[3];
            string op2 = match[4];

            if (values.count(op1)) op1 = values[op1];
            if (values.count(op2)) op2 = values[op2];

            if (
                regex_match(op1, regex(R"(\d+)")) &&
                regex_match(op2, regex(R"(\d+)"))
            ) {
                // Constant folding
                int a = stoi(op1);
                int b = stoi(op2);

                int result = 0;

                if      (op == "+")           result = a + b;
                else if (op == "-")           result = a - b;
                else if (op == "*")           result = a * b;
                else if (op == "/" && b != 0) result = a / b;

                values[lhs] = to_string(result);

                optimizedCode.push_back(
                    lhs + " = " + to_string(result)
                );
            }
            else {
                optimizedCode.push_back(
                    lhs + " = " + op1 + " " + op + " " + op2
                );
            }
        }

        // Comparison expressions — kept as-is (cannot fold), but
        // propagate value substitutions for operands
        else if (regex_match(line, match, cmpPattern)) {

            string lhs = match[1];
            string op1 = match[2];
            string op  = match[3];
            string op2 = match[4];

            if (values.count(op1)) op1 = values[op1];
            if (values.count(op2)) op2 = values[op2];

            optimizedCode.push_back(
                lhs + " = " + op1 + " " + op + " " + op2
            );
        }

        else if (regex_match(line, match, assignPattern)) {

            string lhs = match[1];
            string rhs = match[2];

            if (values.count(rhs)) rhs = values[rhs];

            values[lhs] = rhs;

            if (lhs != rhs) {
                optimizedCode.push_back(lhs + " = " + rhs);
            }
        }

        else {
            // Control-flow lines (labels, goto, ifFalse, ifTrue) — keep verbatim
            optimizedCode.push_back(line);
        }
    }

    // ---- Dead-code elimination: remove unused temporaries ----
    set<string> usedVariables;

    vector<string> finalOptimized;

    regex assignPattern2(
        R"((\w+)\s*=\s*(.*))"
    );

    for (int i = (int)optimizedCode.size() - 1; i >= 0; i--) {

        string line = optimizedCode[i];

        smatch match;

        if (regex_match(line, match, assignPattern2)) {

            string lhs = match[1];
            string rhs = match[2];

            if (usedVariables.count(lhs) || lhs[0] != 't') {

                finalOptimized.push_back(line);

                regex varPattern(
                    R"([a-zA-Z_][a-zA-Z0-9_]*)"
                );

                auto begin = sregex_iterator(rhs.begin(), rhs.end(), varPattern);
                auto end   = sregex_iterator();

                for (auto j = begin; j != end; ++j) {

                    string var = j->str();

                    if (!regex_match(var, regex(R"(\d+)"))) {
                        usedVariables.insert(var);
                    }
                }
            }
        }
        else {
            // Control-flow lines always kept; mark any identifiers in them as used
            finalOptimized.push_back(line);

            regex varPattern(R"([a-zA-Z_][a-zA-Z0-9_]*)");

            auto begin = sregex_iterator(line.begin(), line.end(), varPattern);
            auto end   = sregex_iterator();

            for (auto j = begin; j != end; ++j) {
                string var = j->str();
                if (!regex_match(var, regex(R"(\d+)"))) {
                    usedVariables.insert(var);
                }
            }
        }
    }

    reverse(finalOptimized.begin(), finalOptimized.end());

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

    hasError     = false;
    errorMessage = "";

    tokens = lexicalAnalysis(code);

    pos = 0;

    parserSteps.clear();
    treeSteps.clear();

    intermediateCode.clear();
    optimizedCode.clear();
    finalCode.clear();

    tempCount  = 1;
    labelCount = 1;

    // ---------------- TOKENS ----------------
    cout << "=== TOKENS ===\n";

    for (auto &t : tokens) {
        cout << t.lexeme << " -> " << t.type << endl;
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
        cout << "STEP " << step++ << ":\n";
        cout << t << "\n";
    }

    // ---------------- FINAL PARSE TREE ----------------
    string finalTree = "";

    buildTreeString(tree, "", true, finalTree);

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
