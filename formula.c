#include "formula.h"
#include "eval.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>


static Formula* node_number(double val) {
    Formula* f = (Formula*)malloc(sizeof(Formula));
    if (!f)
        return NULL;
    f->type = NODE_NUMBER;
    f->number = val;
    return f;
}


static Formula* node_cell(int row, int col) {
    Formula* f = (Formula*)malloc(sizeof(Formula));
    if (!f)
        return NULL;
    f->type = NODE_CELL;
    f->cell.row = row;
    f->cell.col = col;
    return f;
}


static Formula* node_range(int r1, int c1, int r2, int c2) {
    Formula* f = (Formula*)malloc(sizeof(Formula));
    if (!f)
        return NULL;
    f->type = NODE_RANGE;
    f->range.r1 = r1;
    f->range.c1 = c1;
    f->range.r2 = r2;
    f->range.c2 = c2;
    return f;
}


static Formula* node_binary(NodeType type, Formula* left, Formula* right) {
    Formula* f = (Formula*)malloc(sizeof(Formula));
    if (!f)
        return NULL;
    f->type = type;
    f->binary.left = left;
    f->binary.right = right;
    return f;
}


static Formula* node_func(NodeType type, Formula* child) {
    Formula* f = (Formula*)malloc(sizeof(Formula));
    if (!f)
        return NULL;
    f->type = type;
    f->unary.child = child;
    return f;
}


static int col_to_idx(const char* col) {
    int idx = 0;
    while (isalpha(*col)) {
        idx = idx * 26 + (toupper(*col) - 'A' + 1);
        col++;
    }
    idx--;
    return idx;
}


static void parse_ref(const char* s, int* row, int* col) {
    *col = col_to_idx(s);
    while (isalpha(*s))
        s++;
    *row = atoi(s) - 1;
}


static Formula* parse_expr(const char** s);


static void skip_spaces(const char** s) {
    while (isspace(**s))
        (*s)++;
}


static Formula* parse_primary(const char** s) {
    skip_spaces(s);

    if (**s == '-') {
        (*s)++;
        Formula* operand = parse_primary(s);
        if (!operand)
            return NULL;
        Formula* zero = node_number(0.0);
        if (!zero) {
            formula_free(operand);
            return NULL;
        }
        return node_binary(NODE_SUB, zero, operand);
    }

    if (**s == '+') {
        (*s)++;
        return parse_primary(s);
    }

    if (isdigit(**s) || (**s == '.')) {
        char* end;
        double val = strtod(*s, &end);
        *s = end;
        return node_number(val);
    }

    if (isalpha(**s)) {
        char ref[32];
        int i = 0;
        while (isalnum(**s) && i < 31)
            ref[i++] = *(*s)++;
        ref[i] = '\0';

        if ((toupper(ref[0]) == 'S' && toupper(ref[1]) == 'U' && toupper(ref[2]) == 'M') ||
            (toupper(ref[0]) == 'M' && toupper(ref[1]) == 'A' && toupper(ref[2]) == 'X') ||
            (toupper(ref[0]) == 'M' && toupper(ref[1]) == 'I' && toupper(ref[2]) == 'N')) {
            skip_spaces(s);
            if (**s == '(') {
                (*s)++;
                skip_spaces(s);

                char range_str[64];
                i = 0;
                while (**s && **s != ')' && i < 63)
                    range_str[i++] = *(*s)++;
                range_str[i] = '\0';

                int r1, c1, r2, c2;
                char* colon = strchr(range_str, ':');
                if (colon) {
                    *colon = '\0';
                    parse_ref(range_str, &r1, &c1);
                    parse_ref(colon + 1, &r2, &c2);
                }
                else {
                    parse_ref(range_str, &r1, &c1);
                    r2 = r1;
                    c2 = c1;
                }

                Formula* range_node = node_range(r1, c1, r2, c2);
                if (!range_node)
                    return NULL;

                skip_spaces(s);
                if (**s == ')')
                    (*s)++;

                NodeType func_type;
                if (toupper(ref[0]) == 'S' && toupper(ref[1]) == 'U' && toupper(ref[2]) == 'M')
                    func_type = NODE_FUNC_SUM;
                else {
                    if (toupper(ref[0]) == 'M' && toupper(ref[1]) == 'I' && toupper(ref[2]) == 'N')
                        func_type = NODE_FUNC_MIN;
                    else
                        func_type = NODE_FUNC_MAX;
                }
                return node_func(func_type, range_node);
            }
        }
        else {
            *s -= i;
            int row, col;
            parse_ref(*s, &row, &col);
            while (isalnum(**s))
                (*s)++;
            return node_cell(row, col);
        }
    }

    if (**s == '(') {
        (*s)++;
        Formula* f = parse_expr(s);
        skip_spaces(s);
        if (**s == ')')
            (*s)++;
        return f;
    }

    return NULL;
}


static Formula* parse_term(const char** s) {
    Formula* left = parse_primary(s);
    if (!left)
        return NULL;

    while (1) {
        skip_spaces(s);
        if (**s == '*') {
            (*s)++;
            Formula* right = parse_primary(s);
            if (!right) {
                formula_free(left);
                return NULL;
            }
            left = node_binary(NODE_MUL, left, right);
            if (!left) {
                formula_free(right);
                return NULL;
            }
        }
        else {
            if (**s == '/') {
                (*s)++;
                Formula* right = parse_primary(s);
                if (!right) {
                    formula_free(left);
                    return NULL;
                }
                left = node_binary(NODE_DIV, left, right);
                if (!left) {
                    formula_free(right);
                    return NULL;
                }
            }
            else
                break;
        }
    }
    return left;
}


static Formula* parse_expr(const char** s) {
    Formula* left = parse_term(s);
    if (!left)
        return NULL;

    while (1) {
        skip_spaces(s);
        if (**s == '+') {
            (*s)++;
            Formula* right = parse_term(s);
            if (!right) {
                formula_free(left);
                return NULL;
            }
            left = node_binary(NODE_ADD, left, right);
            if (!left) {
                formula_free(right);
                return NULL;
            }
        }
        else {
            if (**s == '-') {
                (*s)++;
                Formula* right = parse_term(s);
                if (!right) {
                    formula_free(left);
                    return NULL;
                }
                left = node_binary(NODE_SUB, left, right);
                if (!left) {
                    formula_free(right);
                    return NULL;
                }
            }
            else
                break;
        }
        
    }
    return left;
}


Formula* formula_parse(const char* expr) {
    const char* s = expr;
    Formula* f = parse_expr(&s);
    if (!f)
        return NULL;

    skip_spaces(&s);
    if (*s != '\0') {
        formula_free(f);
        return NULL;
    }
    return f;
}


void formula_free(Formula* f) {
    if (!f)
        return;

    switch (f->type) {
    case NODE_ADD:
    case NODE_SUB:
    case NODE_MUL:
    case NODE_DIV:
        formula_free(f->binary.left);
        formula_free(f->binary.right);
        break;
    case NODE_FUNC_SUM:
    case NODE_FUNC_MIN:
    case NODE_FUNC_MAX:
        formula_free(f->unary.child);
        break;
    default:
        break;
    }
    free(f);
}


double formula_evaluate(Formula* f, void* ctx) {
    if (!f)
        return 0.0;

    EvalContext* ectx = (EvalContext*)ctx;
    if (!ectx || !ectx->get_value)
        return 0.0;

    switch (f->type) {
    case NODE_NUMBER:
        return f->number;
    case NODE_CELL:
        return ectx->get_value(ectx->user_data, f->cell.row, f->cell.col);
    case NODE_RANGE:
        return 0.0;
    case NODE_ADD:
        return formula_evaluate(f->binary.left, ctx) + formula_evaluate(f->binary.right, ctx);
    case NODE_SUB:
        return formula_evaluate(f->binary.left, ctx) - formula_evaluate(f->binary.right, ctx);
    case NODE_MUL:
        return formula_evaluate(f->binary.left, ctx) * formula_evaluate(f->binary.right, ctx);
    case NODE_DIV: {
        double left = formula_evaluate(f->binary.left, ctx);
        double right = formula_evaluate(f->binary.right, ctx);
        if (right == 0.0) {
            if (left == 0.0)
                return NAN;
            if (left > 0)
                return INFINITY;
            return -INFINITY;
        }
        return left / right;
    }
    case NODE_FUNC_SUM: {
        Formula* range = f->unary.child;
        if (!range || range->type != NODE_RANGE)
            return 0.0;
        double sum = 0.0;
        for (int r = range->range.r1; r <= range->range.r2; r++)
            for (int c = range->range.c1; c <= range->range.c2; c++)
                sum += ectx->get_value(ectx->user_data, r, c);
        return sum;
    }
    case NODE_FUNC_MIN: {
        Formula* range = f->unary.child;
        if (!range || range->type != NODE_RANGE)
            return 0.0;
        double minv = INFINITY;
        int first = 1;
        for (int r = range->range.r1; r <= range->range.r2; r++)
            for (int c = range->range.c1; c <= range->range.c2; c++) {
                double val = ectx->get_value(ectx->user_data, r, c);
                if (first || val < minv) {
                    minv = val;
                    first = 0;
                }
            }
        return first ? 0.0 : minv;
    }
    case NODE_FUNC_MAX: {
        Formula* range = f->unary.child;
        if (!range || range->type != NODE_RANGE)
            return 0.0;
        double maxv = -INFINITY;
        int first = 1;
        for (int r = range->range.r1; r <= range->range.r2; r++)
            for (int c = range->range.c1; c <= range->range.c2; c++) {
                double val = ectx->get_value(ectx->user_data, r, c);
                if (first || val > maxv) {
                    maxv = val;
                    first = 0;
                }
            }
        return first ? 0.0 : maxv;
    }
    default:
        return 0.0;
    }
}
