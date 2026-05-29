#ifndef CSV_H
#define CSV_H


#include "spreadsheet.h"


int csv_read(Sheet* s, const char* filename);
int csv_write(Sheet* s, const char* filename);


#endif
