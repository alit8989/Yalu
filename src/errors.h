#ifndef YALU_ERRORS_H
#define YALU_ERRORS_H

#include "common.h"

extern int g_yalu_error_count;
extern bool g_yalu_panic_mode;

void yalu_errors_reset(void);
void yalu_report_error(SourcePos pos, const char* stage, const char* fmt, ...);
void yalu_report_warning(SourcePos pos, const char* fmt, ...);

/* Sets the active source buffer to show line excerpts in error messages */
void yalu_set_source_buffer(const char* source);

#endif /* YALU_ERRORS_H */

