#include "csv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static char* read_line(FILE* f) {
    char* line = NULL;
    size_t size = 0;
    size_t len = 0;
    int c;

    c = fgetc(f);
    if (c == EOF)
        return NULL;

    ungetc(c, f);

    while ((c = fgetc(f)) != EOF) {
        if (len + 1 >= size) {
            size_t new_size = size ? size * 2 : 128;
            char* new_line = (char*)realloc(line, new_size);
            if (!new_line) {
                free(line);
                return NULL;
            }
            line = new_line;
            size = new_size;
        }
        if (c == '\n')
            break;
        line[len++] = (char)c;
    }

    if (len == 0 && c == EOF) {
        free(line);
        return NULL;
    }

    if (len + 1 >= size) {
        char* new_line = (char*)realloc(line, len + 1);
        if (!new_line) {
            free(line);
            return NULL;
        }
        line = new_line;
    }
    line[len] = '\0';

    return line;
}


int csv_read(Sheet* s, const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f)
        return -1;

    char* line;
    int row = 0;
    while ((line = read_line(f)) != NULL) {
        char* str = line;
        int col = 0;
        char* start = str;

        while (*str) {
            if (*str == ',') {
                *str = '\0';
                if (strlen(start) > 0)
                    sheet_set_cell(s, row, col, start);
                start = str + 1;
                col++;
            }
            str++;
        }
        if (strlen(start) > 0)
            sheet_set_cell(s, row, col, start);

        free(line);
        row++;
    }

    fclose(f);
    return 0;
}


int csv_write(Sheet* s, const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f)
        return -1;

    sheet_evaluate(s);

    int rows = sheet_get_rows(s);
    int cols = sheet_get_cols(s);

    int max_row = -1;
    int max_col = -1;
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++) {
            const char* raw = sheet_get_cell_raw(s, i, j);
            if (raw && raw[0] != '\0') {
                if (i > max_row)
                    max_row = i;
                if (j > max_col)
                    max_col = j;
            }
        }

    if (max_row < 0)
        max_row = 0;
    if (max_col < 0)
        max_col = 0;

    for (int i = 0; i <= max_row; i++) {
        int printed = 0;
        for (int j = 0; j <= max_col; j++) {
            double val = sheet_get_cell_value(s, i, j);
            const char* raw = sheet_get_cell_raw(s, i, j);

            if (raw && raw[0] != '\0') {
                if (printed)
                    fprintf(f, ",");

                if (val == (int)val)
                    fprintf(f, "%d", (int)val);
                else
                    fprintf(f, "%g", val);

                printed = 1;
            }
            else {
                if (printed)
                    fprintf(f, ",");
            }
        }
        if (printed)
            fprintf(f, "\n");
    }

    fclose(f);
    return 0;
}
