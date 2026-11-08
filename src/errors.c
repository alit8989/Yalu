#include "errors.h"

int g_yalu_error_count = 0;
bool g_yalu_panic_mode = false;
static const char* s_active_source = NULL;

void yalu_errors_reset(void) {
    g_yalu_error_count = 0;
    g_yalu_panic_mode = false;
    s_active_source = NULL;
}

void yalu_set_source_buffer(const char* source) {
    s_active_source = source;
}

static void print_source_line(int target_line, int target_col) {
    if (!s_active_source || target_line <= 0) return;

    int current_line = 1;
    const char* p = s_active_source;
    const char* line_start = p;

    while (*p) {
        if (current_line == target_line) {
            line_start = p;
            while (*p && *p != '\n' && *p != '\r') {
                p++;
            }
            int line_len = (int)(p - line_start);
            fprintf(stderr, "    | %.*s\n", line_len, line_start);
            fprintf(stderr, "    | ");
            for (int i = 1; i < target_col; i++) {
                fprintf(stderr, " ");
            }
            fprintf(stderr, "^\n");
            return;
        }
        if (*p == '\n') {
            current_line++;
        }
        p++;
    }
}

void yalu_report_error(SourcePos pos, const char* stage, const char* fmt, ...) {
    g_yalu_error_count++;

    fprintf(stderr, "[Yalu %s] %s:%d:%d: ",
            stage ? stage : "Error",
            pos.filename ? pos.filename : "<input>",
            pos.line,
            pos.col);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");

    print_source_line(pos.line, pos.col);
}

void yalu_report_warning(SourcePos pos, const char* fmt, ...) {
    fprintf(stderr, "[Yalu Warning] %s:%d:%d: ",
            pos.filename ? pos.filename : "<input>",
            pos.line,
            pos.col);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");

    print_source_line(pos.line, pos.col);
}

