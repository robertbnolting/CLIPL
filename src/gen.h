void gen();

enum MnemType {
	MOV = 1,
	LEA,
	ADD,
	SUB,
	IMUL,
	AND,
	OR,
	SHR,
	SHL,
	CMP,
	INC,
	DEC,
	DIV,
	NEG,
	NOT,
	JE,
	JNE,
	JL,
	JLE,
	JG,
	JGE,
	JMP,
	GOTO,
	CALL,
	PUSH,
	POP,
	RET,
	BRACKET_EXPR,
	BRACKET_ADD,
	VIRTUAL_REG,
	REAL_REG,
	LABEL,
	LITERAL,
	SPECIFIER,
	SYSCALL,
	NEWLINE,
};

typedef struct MnemNode {
	int type;
	char *mnem;
	struct MnemNode *left;
	struct MnemNode *right;

	// specifiers like qword, dword, ...
	struct MnemNode *left_spec;
	struct MnemNode *right_spec;

	// only for BRACKET_EXPR
	struct MnemNode **vregs_used;
	int n_vregs_used;
	// only for virtual registers
	int first_def;
	int ret_belongs_to;
	int is_function_label;
	int in_loop;
	int idx;
	char mode;
} MnemNode;

typedef struct InterferenceNode {
	int idx;
	struct InterferenceNode **neighbors;
	size_t neighbor_count;
	// used in coloring
	int color;
	int saturation;
} InterferenceNode;
