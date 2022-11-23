void gen();

typedef struct MnemNode {
	char *mnem;
	struct MnemNode *left;
	struct MnemNode *right;
} MnemNode;

typedef struct InterferenceNode {
	int idx;
	struct InterferenceNode **neighbors;
	size_t neighbor_count;
} InterferenceNode;
