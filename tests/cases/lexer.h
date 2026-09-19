// A comment mentioning REFLECT() must not be picked up.
/* Nor a block comment with PROPERTY(range = {1, 2}) inside it. */

REFLECT()
struct Lexing {
    // Digit separators must not desynchronise the lexer into a char literal.
    PROPERTY() int million = 1'000'000;
    PROPERTY() int hexed = 0xFF'FF;
    PROPERTY() char quote = '\'';
    PROPERTY() char backslash = '\\';

    // Annotation names inside literals are just text.
    PROPERTY() const char* text = "REFLECT() is not an annotation here";
    PROPERTY() const char* raw = R"sql(PROPERTY() stays put ")sql";
    PROPERTY() const char* plain_raw = R"(also PROPERTY() here)";

    // A declared name used without parens is an ordinary identifier.
    PROPERTY() int REFLECT;
};
