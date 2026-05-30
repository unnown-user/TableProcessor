#include "spreadsheet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INT_MAX 0x7FFFFFFF


void print_help(const char* progname) {
    printf("============================================================\n");
    printf("  Spreadsheet Processor - CSV Table Calculator\n");
    printf("============================================================\n");
    printf("\n");
    printf("NAME\n");
    printf("  %s - evaluate formulas in CSV spreadsheets\n", progname);
    printf("\n");
    printf("SYNOPSIS\n");
    printf("  %s [OPTIONS]\n", progname);
    printf("\n");
    printf("DESCRIPTION\n");
    printf("  Spreadsheet processor reads CSV files, evaluates formulas,\n");
    printf("  and writes results back to CSV format. Supports arithmetic\n");
    printf("  operations, cell references, and aggregate functions.\n");
    printf("\n");
    printf("OPTIONS\n");
    printf("  -i, --input FILE\n");
    printf("      Read input CSV file\n");
    printf("\n");
    printf("  -o, --output FILE\n");
    printf("      Write result to output CSV file (formulas are evaluated)\n");
    printf("\n");
    printf("  -e, --eval\n");
    printf("      Print result to console in CSV format\n");
    printf("\n");
    printf("  -s, --set CELL=EXPRESSION\n");
    printf("      Set cell content. CELL format: A1, B2, AA10\n");
    printf("      EXPRESSION can be a number or a formula starting with '='\n");
    printf("\n");
    printf("  -h, --help\n");
    printf("      Display this help message\n");
    printf("\n");
    printf("FORMULA SYNTAX\n");
    printf("  Numbers:         123, 45.67\n");
    printf("  Cell references: A1, B2, Z100, AA10\n");
    printf("  Arithmetic:      +  -  *  /  ()\n");
    printf("  Unary minus:     -A1, -(B2+C3)\n");
    printf("  Functions:       SUM(A1:A10), MIN(B1:B20), MAX(C1:C30)\n");
    printf("\n");
    printf("EXAMPLES\n");
    printf("  %s -h\n", progname);
    printf("      Show this help\n");
    printf("\n");
    printf("  %s -s A1=10 -s B1=20 -s C1=\"=A1+B1\" -e\n", progname);
    printf("      Create a small table and print result\n");
    printf("\n");
    printf("  %s -i data.csv -o result.csv\n", progname);
    printf("      Read data.csv, evaluate formulas, save to result.csv\n");
    printf("\n");
    printf("  %s -i data.csv -e\n", progname);
    printf("      Read data.csv, evaluate formulas, print to console\n");
    printf("\n");
    printf("  %s -s A1=5 -s A2=10 -s A3=15 -s B1=\"=SUM(A1:A3)\" -o out.csv\n", progname);
    printf("      Create table with SUM function and save to file\n");
    printf("\n");
    printf("NOTES\n");
    printf("  - Formulas must start with '=' (e.g., \"=A1+B2\")\n");
    printf("  - Aggregate functions work with ranges: SUM(A1:A10)\n");
    printf("  - Cyclic dependencies are detected and reported as warnings\n");
    printf("  - Table expands automatically as needed\n");
    printf("  - Empty cells are preserved in output\n");
    printf("============================================================\n");
}


static char* my_strdup(const char* s) {
    if (!s)
        return NULL;
    size_t len = strlen(s) + 1;
    char* new = (char*)malloc(len);
    if (new)
        memcpy(new, s, len);
    return new;
}


static int is_option(const char* arg, const char* short_opt, const char* long_opt) {
    if (short_opt && strcmp(arg, short_opt) == 0)
        return 1;
    if (long_opt && strcmp(arg, long_opt) == 0)
        return 1;
    return 0;
}


static const char* get_option_value(const char* arg, const char* long_opt) {
    if (!arg || !long_opt)
        return NULL;
    size_t opt_len = strlen(long_opt);
    if (strncmp(arg, long_opt, opt_len) == 0 && arg[opt_len] == '=')
        return arg + opt_len + 1;
    return NULL;
}


static int parse_cell_ref(const char* ref, int* row, int* col) {
    if (!ref || !row || !col)
        return -1;

    int col_len = 0;
    while (isalpha((unsigned char)ref[col_len]))
        col_len++;

    if (col_len == 0)
        return -1;
    if (ref[col_len] == '\0')
        return -1;

    *row = atoi(ref + col_len) - 1;
    if (*row < 0)
        return -1;

    *col = 0;
    for (int j = 0; j < col_len; j++) {
        char c = toupper((unsigned char)ref[j]);
        if (c < 'A' || c > 'Z')
            return -1;

        if (*col > (INT_MAX - (c - 'A' + 1)) / 26)
            return -1;

        *col = *col * 26 + (c - 'A' + 1);
    }
    *col = *col - 1;

    return 0;
}


static void print_spreadsheet(Sheet* s) {
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
        for (int j = 0; j <= max_col; j++) {
            double val = sheet_get_cell_value(s, i, j);
            const char* raw = sheet_get_cell_raw(s, i, j);

            if (j > 0)
                printf(",");

            if (raw && raw[0] != '\0') {
                if (val == (int)val)
                    printf("%d", (int)val);
                else
                    printf("%g", val);
            }
        }
        printf("\n");
    }
}


int main(int argc, char** argv) {
    char* input_file = NULL;
    char* output_file = NULL;
    int eval_only = 0;
    char** set_commands = NULL;
    int set_count = 0;

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            print_help(argv[0]);
            for (int j = 0; j < set_count; j++)
                free(set_commands[j]);
            free(set_commands);
            if (input_file)
                free(input_file);
            if (output_file)
                free(output_file);
            return 0;
        }
        else if (strcmp(arg, "--eval") == 0 || strcmp(arg, "-e") == 0) {
            eval_only = 1;
        }
        else if (is_option(arg, "-i", "--input")) {
            const char* value = get_option_value(arg, "--input");
            if (!value && i + 1 < argc)
                value = argv[++i];
            if (!value) {
                fprintf(stderr, "Error: --input/-i requires a filename\n");
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                for (int j = 0; j < set_count; j++)
                    free(set_commands[j]);
                free(set_commands);
                return 1;
            }
            if (input_file)
                free(input_file);
            input_file = my_strdup(value);
            if (!input_file) {
                fprintf(stderr, "Error: Memory allocation failed\n");
                for (int j = 0; j < set_count; j++)
                    free(set_commands[j]);
                free(set_commands);
                return 1;
            }
        }
        else if (is_option(arg, "-o", "--output")) {
            const char* value = get_option_value(arg, "--output");
            if (!value && i + 1 < argc)
                value = argv[++i];
            if (!value) {
                fprintf(stderr, "Error: --output/-o requires a filename\n");
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                for (int j = 0; j < set_count; j++)
                    free(set_commands[j]);
                free(set_commands);
                if (input_file)
                    free(input_file);
                return 1;
            }
            if (output_file)
                free(output_file);
            output_file = my_strdup(value);
            if (!output_file) {
                fprintf(stderr, "Error: Memory allocation failed\n");
                for (int j = 0; j < set_count; j++)
                    free(set_commands[j]);
                free(set_commands);
                if (input_file)
                    free(input_file);
                return 1;
            }
        }
        else if (is_option(arg, "-s", "--set")) {
            const char* value = get_option_value(arg, "--set");
            if (!value && i + 1 < argc)
                value = argv[++i];
            if (!value) {
                fprintf(stderr, "Error: --set/-s requires a value (CELL=expression)\n");
                fprintf(stderr, "Example: --set A1=10 or --set B2=\"=A1*2\"\n");
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                for (int j = 0; j < set_count; j++)
                    free(set_commands[j]);
                free(set_commands);
                if (input_file)
                    free(input_file);
                if (output_file)
                    free(output_file);
                return 1;
            }

            char** new_commands = (char**)realloc(set_commands, (set_count + 1) * sizeof(char*));
            if (!new_commands) {
                fprintf(stderr, "Error: Memory allocation failed\n");
                for (int j = 0; j < set_count; j++)
                    free(set_commands[j]);
                free(set_commands);
                if (input_file)
                    free(input_file);
                if (output_file)
                    free(output_file);
                return 1;
            }
            set_commands = new_commands;
            set_commands[set_count++] = my_strdup(value);
            if (!set_commands[set_count - 1]) {
                fprintf(stderr, "Error: Memory allocation failed\n");
                for (int j = 0; j < set_count - 1; j++)
                    free(set_commands[j]);
                free(set_commands);
                if (input_file)
                    free(input_file);
                if (output_file)
                    free(output_file);
                return 1;
            }
        }
        else if (arg[0] == '-') {
            fprintf(stderr, "\nError: Unknown option '%s'\n", arg);
            fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
            for (int j = 0; j < set_count; j++)
                free(set_commands[j]);
            free(set_commands);
            if (input_file)
                free(input_file);
            if (output_file)
                free(output_file);
            return 1;
        }
        else {
            fprintf(stderr, "Warning: Ignoring positional argument '%s'\n", arg);
        }
    }

    if (!input_file && !eval_only && set_count == 0 && !output_file) {
        fprintf(stderr, "\nError: No input, output, or evaluation specified.\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        for (int j = 0; j < set_count; j++)
            free(set_commands[j]);
        free(set_commands);
        if (input_file)
            free(input_file);
        if (output_file)
            free(output_file);
        return 1;
    }

    Sheet* s = sheet_create();
    if (!s) {
        fprintf(stderr, "Error: Cannot create spreadsheet.\n");
        for (int j = 0; j < set_count; j++)
            free(set_commands[j]);
        free(set_commands);
        if (input_file)
            free(input_file);
        if (output_file)
            free(output_file);
        return 1;
    }

    if (input_file) {
        if (sheet_load_csv(s, input_file) != 0) {
            fprintf(stderr, "Error: Cannot read input file '%s'\n", input_file);
            sheet_destroy(s);
            for (int j = 0; j < set_count; j++)
                free(set_commands[j]);
            free(set_commands);
            if (input_file)
                free(input_file);
            if (output_file)
                free(output_file);
            return 1;
        }
    }

    for (int i = 0; i < set_count; i++) {
        char* eq = strchr(set_commands[i], '=');
        if (!eq) {
            fprintf(stderr, "Error: Invalid set format '%s'. Use CELL=value\n", set_commands[i]);
            continue;
        }

        *eq = '\0';
        char* cell_ref = set_commands[i];
        char* content = eq + 1;

        int row, col;
        if (parse_cell_ref(cell_ref, &row, &col) != 0) {
            fprintf(stderr, "Error: Invalid cell reference '%s'\n", cell_ref);
            *eq = '=';
            continue;
        }

        if (sheet_set_cell(s, row, col, content) != 0)
            fprintf(stderr, "Error: Cannot set cell %s (invalid formula or memory error)\n", set_commands[i]);

        *eq = '=';
    }

    int eval_result = sheet_evaluate(s);
    if (eval_result != 0) {
        fprintf(stderr, "Warning: Cyclic dependency detected in formulas.\n");
        eval_only = 0;
    }

    if (output_file) {
        sheet_save_csv(s, output_file);
        printf("Saved result to %s\n", output_file);
    }

    if (eval_only)
        print_spreadsheet(s);

    sheet_destroy(s);

    for (int j = 0; j < set_count; j++)
        free(set_commands[j]);
    free(set_commands);
    if (input_file)
        free(input_file);
    if (output_file)
        free(output_file);

    return 0;
}
