#ifndef FORMULA_H
#define FORMULA_H

#include "eval.h"


typedef struct Formula Formula;

typedef enum {
    NODE_NUMBER,
    NODE_CELL,
    NODE_RANGE,
    NODE_ADD, NODE_SUB, NODE_MUL, NODE_DIV,
    NODE_FUNC_SUM, NODE_FUNC_MIN, NODE_FUNC_MAX
} NodeType;

struct Formula {
    NodeType type;
    union {
        double number;
        struct {
            int row;
            int col;
        } cell;
        struct {
            int r1, c1, r2, c2;
        } range;
        struct {
            Formula* left;
            Formula* right;
        } binary;
        struct {
            Formula* child;
        } unary;
    };
};


Formula* formula_parse(const char* expr);
void formula_free(Formula* f);
double formula_evaluate(Formula* f, void* ctx);


#endif
