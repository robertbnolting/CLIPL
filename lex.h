enum {
	ERRONEOUS = 256,
	EoF,
	IDENTIFIER,
	INT,
	STRING,
	BINARY_ARITH,
	OPEN_BRACE,
	CLOSE_BRACE,
	COMMA,
	DOT,
	SEMICOLON,
	ONE_CHAR_ASSIGNMENT,
	TWO_CHAR_ASSIGNMENT,
	TWO_CHAR_COMPARE
};

#define get_bits(ch) (charbits[(ch)&0377])

#define UC_LETTER_MASK (1 << 1) // 0002 2
#define LC_LETTER_MASK (1 << 2) // 0004 4
#define DIGIT_MASK (1 << 3)	// 0010 8
#define OPERATOR_MASK (1 << 4)  // 0020 16
#define SEPARATOR_MASK (1 << 5) // 0040 32
#define LETTER_MASK (UC_LETTER_MASK | LC_LETTER_MASK)

#define is_end_of_file(ch) 	(ch == '\0')
#define is_layout(ch) 		(!is_end_of_file(ch) && ch <= ' ')
#define is_comment_starter(ch) 	(ch == '#')
#define is_comment_stopper(ch)	(ch == '#' || ch == '\n')
#define is_underscore(ch)	(ch == '_')
#define is_uc_letter(ch)	(get_bits(ch) & UC_LETTER_MASK)
#define is_lc_letter(ch)	(get_bits(ch) & LC_LETTER_MASK)
#define is_letter(ch)		(get_bits(ch) & LETTER_MASK)
#define is_digit(ch)		(get_bits(ch) & DIGIT_MASK)
#define is_operator(ch)		(get_bits(ch) & OPERATOR_MASK)
#define is_separator(ch)	(get_bits(ch) & SEPARATOR_MASK)

static const char charbits[256] = {
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
	0000, 0000, 0000, 0020, 0000, 0000, 0000, 0000, 0000, 0000, 
	0040, 0040, 0020, 0020, 0040, 0020, 0040, 0020, 0010, 0010, 
	0010, 0010, 0010, 0010, 0010, 0010, 0010, 0010, 0000, 0040, 
	0000, 0020, 0000, 0000, 0000, 0002, 0002, 0002, 0002, 0002, 
	0002, 0002, 0002, 0002, 0002, 0002, 0002, 0002, 0002, 0002, 
	0002, 0002, 0002, 0002, 0002, 0002, 0002, 0002, 0002, 0002, 
	0002, 0040, 0000, 0040, 0000, 0000, 0000, 0004, 0004, 0004, 
	0004, 0004, 0004, 0004, 0004, 0004, 0004, 0004, 0004, 0004, 
	0004, 0004, 0004, 0004, 0004, 0004, 0004, 0004, 0004, 0004, 
	0004, 0004, 0004, 0040, 0000, 0040, 0000, 0000, 0000, 0000, 
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 
	0000, 0000, 0000, 0000, 0000, 0000
};


typedef struct
{
	char *filename;
	int line;
	int column;
} File_pos;

typedef struct
{
	int class;
	char *repr;
	File_pos pos;
} Token_type;

extern Token_type Token;

void start_lexer();
void get_next_token();

static void skip_layout_and_comments();
