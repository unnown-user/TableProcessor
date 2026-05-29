#ifndef EVAL_H
#define EVAL_H


typedef struct EvalContext {
    double (*get_value)(void* user_data, int row, int col);
    void* user_data;
    int* cycle_detected;
} EvalContext;


#endif
