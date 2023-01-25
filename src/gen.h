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
	PUSH,
	POP,
	BRACKET_EXPR,
	BRACKET_ADD,
	VIRTUAL_REG,
	REAL_REG,
	LABEL,
	LITERAL,
	SPECIFIER,
	SYSCALL,
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
