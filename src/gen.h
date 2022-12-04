void gen();

enum MnemType {
	MOV = 1,
	ADD,
	CMP,
	INC,
	JE,
	JNE,
	JZ,
	JNZ,
	JMP,
	GOTO,
	PUSH,
	POP,
	BRACKET_EXPR,
	BRACKET_ADD,
	VIRTUAL_REG,
	LABEL,
};

typedef struct MnemNode {
	int type;
	char *mnem;
	struct MnemNode *left;
	struct MnemNode *right;
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
