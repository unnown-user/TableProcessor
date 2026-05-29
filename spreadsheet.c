#include "spreadsheet.h"
#include "formula.h"
#include "eval.h"
#include "csv.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define INITIAL_ROWS 100
#define INITIAL_COLS 200
#define GROWTH_FACTOR 2


struct Cell {
    CellType type;       // Cell type: number, formula, or empty
    char* raw_content;   // Original text (for example, "=A1+B2")
    double value;        // Calculated result
    Formula* formula;    // Pointer to the formula tree
    int visited;         // Flag: has this cell already been calculated or not
    int computing;       // Flag: is this cell being calculated right now or not (to detect cycles)
};

struct Sheet {
    Cell*** cells;       // Table[row][column] -> Cell
    int rows;            // Current number of rows
    int cols;            // Current number of columns
    int eval_required;   // Flag: do we need to recalculate formulas or not
};

typedef struct {
    Sheet* sheet;
    int cycle_detected;
    int recursion_depth;
} EvalState;


static char* my_strdup(const char* s) {
    if (!s)
        return NULL;
    size_t len = strlen(s) + 1;
    char* new = (char*)malloc(len);
    if (new)
        memcpy(new, s, len);
    return new;
}


static double eval_cell_with_state(Sheet* s, int row, int col, EvalState* state);


static double get_cell_value_callback(void* user_data, int row, int col) {
    EvalState* state = (EvalState*)user_data;
    if (!state || state->cycle_detected)
        return 0.0;
    return eval_cell_with_state(state->sheet, row, col, state);
}


Sheet* sheet_create(void) {
    Sheet* s = (Sheet*)malloc(sizeof(Sheet));
    if (!s)
        return NULL;

    s->rows = INITIAL_ROWS;
    s->cols = INITIAL_COLS;
    s->eval_required = 1;

    s->cells = (Cell***)calloc(s->rows, sizeof(Cell**));
    if (!s->cells) {
        free(s);
        return NULL;
    }

    for (int i = 0; i < s->rows; i++) {
        s->cells[i] = (Cell**)calloc(s->cols, sizeof(Cell*));
        if (!s->cells[i]) {
            for (int j = 0; j < i; j++)
                free(s->cells[j]);
            free(s->cells);
            free(s);
            return NULL;
        }
    }

    return s;
}


static void cell_free(Cell* c) {
    if (!c)
        return;
    if (c->raw_content) {
        free(c->raw_content);
        c->raw_content = NULL;
    }
    if (c->formula) {
        formula_free(c->formula);
        c->formula = NULL;
    }
    free(c);
}


void sheet_destroy(Sheet* s) {
    if (!s)
        return;

    if (s->cells) {
        for (int i = 0; i < s->rows; i++)
            if (s->cells[i]) {
                for (int j = 0; j < s->cols; j++)
                    if (s->cells[i][j])
                        cell_free(s->cells[i][j]);
                free(s->cells[i]);
            }
        free(s->cells);
    }
    free(s);
}


static int ensure_cell(Sheet* s, int row, int col) {
    if (row < s->rows && col < s->cols)
        return 0;

    int new_rows = s->rows;
    int new_cols = s->cols;

    while (row >= new_rows) {
        new_rows *= GROWTH_FACTOR;
        if (new_rows <= 0 || new_rows > 10000000) {
            fprintf(stderr, "Error: Table size too large (max 10M rows)\n");
            return -1;
        }
    }

    while (col >= new_cols) {
        new_cols *= GROWTH_FACTOR;
        if (new_cols <= 0 || new_cols > 10000000) {
            fprintf(stderr, "Error: Table size too large (max 10M columns)\n");
            return -1;
        }
    }

    Cell*** new_cells = (Cell***)calloc(new_rows, sizeof(Cell**));
    if (!new_cells)
        return -1;

    for (int i = 0; i < new_rows; i++) {
        new_cells[i] = (Cell**)calloc(new_cols, sizeof(Cell*));
        if (!new_cells[i]) {
            for (int j = 0; j < i; j++)
                free(new_cells[j]);
            free(new_cells);
            return -1;
        }

        if (i < s->rows) {
            memcpy(new_cells[i], s->cells[i], s->cols * sizeof(Cell*));
            free(s->cells[i]);
            s->cells[i] = NULL;
        }
    }

    free(s->cells);
    s->cells = new_cells;
    s->rows = new_rows;
    s->cols = new_cols;

    return 0;
}


int sheet_set_cell(Sheet* s, int row, int col, const char* content) {
    if (!s || row < 0 || col < 0)
        return -1;

    if (ensure_cell(s, row, col) != 0)
        return -1;

    if (s->cells[row][col]) {
        cell_free(s->cells[row][col]);
        s->cells[row][col] = NULL;
    }

    if (!content || content[0] == '\0') {
        s->eval_required = 1;
        return 0;
    }

    Cell* new_cell = (Cell*)calloc(1, sizeof(Cell));
    if (!new_cell)
        return -1;

    new_cell->raw_content = my_strdup(content);
    if (!new_cell->raw_content) {
        free(new_cell);
        return -1;
    }

    if (content[0] == '=') {
        new_cell->type = CELL_FORMULA;
        new_cell->formula = formula_parse(content + 1);
        if (!new_cell->formula) {
            free(new_cell->raw_content);
            free(new_cell);
            return -1;
        }
        new_cell->visited = 0;
        new_cell->computing = 0;
        new_cell->value = 0.0;
    }
    else {
        char* endptr;
        double val = strtod(content, &endptr);
        if (*endptr == '\0') {
            new_cell->type = CELL_NUMBER;
            new_cell->value = val;
        }
        else
            new_cell->type = CELL_EMPTY;
        new_cell->formula = NULL;
        new_cell->visited = 0;
        new_cell->computing = 0;
    }

    s->cells[row][col] = new_cell;
    s->eval_required = 1;

    return 0;
}


static double eval_cell_with_state(Sheet* s, int row, int col, EvalState* state) {
    if (state->cycle_detected)
        return 0.0;

    if (row < 0 || row >= s->rows || col < 0 || col >= s->cols)
        return 0.0;

    Cell* c = s->cells[row][col];
    if (!c)
        return 0.0;
    if (c->type == CELL_EMPTY)
        return 0.0;
    if (c->type == CELL_NUMBER)
        return c->value;

    if (c->computing) {
        state->cycle_detected = 1;
        return 0.0;
    }

    if (c->visited)
        return c->value;

    c->computing = 1;
    state->recursion_depth++;

    EvalContext ctx;
    ctx.get_value = get_cell_value_callback;
    ctx.user_data = state;
    ctx.cycle_detected = &state->cycle_detected;

    double result = formula_evaluate(c->formula, &ctx);

    state->recursion_depth--;
    c->computing = 0;

    if (!state->cycle_detected) {
        c->value = result;
        c->visited = 1;
    }

    return result;
}


int sheet_evaluate(Sheet* s) {
    if (!s)
        return -1;
    if (!s->eval_required)
        return 0;

    for (int i = 0; i < s->rows; i++)
        for (int j = 0; j < s->cols; j++) {
            Cell* c = s->cells[i][j];
            if (c && c->type == CELL_FORMULA) {
                c->visited = 0;
                c->computing = 0;
            }
        }

    EvalState state;
    state.sheet = s;
    state.cycle_detected = 0;
    state.recursion_depth = 0;

    for (int i = 0; i < s->rows && !state.cycle_detected; i++)
        for (int j = 0; j < s->cols && !state.cycle_detected; j++) {
            Cell* c = s->cells[i][j];
            if (c && c->type == CELL_FORMULA && !c->visited)
                eval_cell_with_state(s, i, j, &state);
        }

    if (state.cycle_detected)
        for (int i = 0; i < s->rows; i++)
            for (int j = 0; j < s->cols; j++) {
                Cell* c = s->cells[i][j];
                if (c && c->type == CELL_FORMULA) {
                    c->value = 0.0;
                    c->visited = 0;
                }
            }

    s->eval_required = 0;
    return state.cycle_detected ? -1 : 0;
}


double sheet_get_cell_value(Sheet* s, int row, int col) {
    if (!s)
        return 0.0;

    EvalState state;
    state.sheet = s;
    state.cycle_detected = 0;
    state.recursion_depth = 0;

    return eval_cell_with_state(s, row, col, &state);
}


const char* sheet_get_cell_raw(Sheet* s, int row, int col) {
    if (!s || row < 0 || row >= s->rows || col < 0 || col >= s->cols)
        return "";
    Cell* c = s->cells[row][col];
    if (!c || !c->raw_content)
        return "";
    return c->raw_content;
}


void sheet_save_csv(Sheet* s, const char* filename) {
    if (!s || !filename)
        return;
    csv_write(s, filename);
}


int sheet_load_csv(Sheet* s, const char* filename) {
    if (!s || !filename)
        return -1;
    return csv_read(s, filename);
}


int sheet_get_rows(Sheet* s) {
    return s ? s->rows : 0;
}


int sheet_get_cols(Sheet* s) {
    return s ? s->cols : 0;
}
