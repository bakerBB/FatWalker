#ifndef __DOUBLY_LINKED_LIST_H_HEADER__
#define __DOUBLY_LINKED_LIST_H_HEADER__

#include "stdint.h"

typedef struct DLL_NODE {
    struct DLL_NODE * next;
    struct DLL_NODE * prev;
} dll_node;

#endif /* __DOUBLY_LINKED_LIST_H_HEADER__ */

