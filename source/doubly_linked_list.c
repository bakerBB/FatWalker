typedef struct DLL_NODE {
    struct DLL_NODE * next;
    struct DLL_NODE * prev;
} dll_node;

typedef struct DLL_LIST {
    struct DLL_NODE * head;
    struct DLL_NODE * tail;
} dll_list;

dllAdd(dll_node* node1, dll_node* node2);