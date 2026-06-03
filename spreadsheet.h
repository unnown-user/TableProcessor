#ifndef SPREADSHEET_H
#define SPREADSHEET_H

#include <stddef.h>


typedef struct Cell Cell;
typedef struct Sheet Sheet;

typedef enum {
    CELL_EMPTY,
    CELL_NUMBER,
    CELL_FORMULA,
    CELL_ERROR_CYCLE,    // for cells that are part of a cyclical relationship
    CELL_ERROR_DIVZERO,  // a/0 (a>0) -> INF; a/0 (a<0) -> -INF
    CELL_ERROR_NAN       // 0/0 -> NaN
} CellType;


Sheet* sheet_create(void);
void sheet_destroy(Sheet* s);

int sheet_set_cell(Sheet* s, int row, int col, const char* content);
const char* sheet_get_cell_raw(Sheet* s, int row, int col);
double sheet_get_cell_value(Sheet* s, int row, int col);
const char* sheet_get_cell_value_str(Sheet* s, int row, int col); int sheet_evaluate(Sheet* s);

int sheet_save_csv(Sheet* s, const char* filename);
int sheet_load_csv(Sheet* s, const char* filename);

int sheet_get_rows(Sheet* s);
int sheet_get_cols(Sheet* s);


#endif
