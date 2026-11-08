#include "common.h"
#include "errors.h"
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "runtime.h"
#include "interpreter.h"

static void print_version(void) {
    printf("Yalu Programming Language [Windows x64]\n");
    printf("Version: %s\n", YALU_VERSION);
    printf("Target: Native Windows Executable\n");
}

static void print_help(void) {
    print_version();
    printf("\nUsage:\n");
    printf("  yalu.exe <file.yl>         Execute a Yalu source file\n");
    printf("  yalu.exe run <file.yl>     Execute a Yalu source file\n");
    printf("  yalu.exe check <file.yl>   Parse and validate syntax without executing\n");
    printf("  yalu.exe --version, -v     Display Yalu version information\n");
    printf("  yalu.exe --help, -h        Display this help message\n");
    printf("  yalu.exe                   Launch interactive Yalu REPL\n");
    printf("\nYalu Syntax Overview:\n");
    printf("  Comments:       <<\"\" comment \"\">>\n");
    printf("  Variables:      x = 10, name = \"hello\", b = Tr, f = Fl\n");
    printf("  Multiline Str:  s = \"\"\\n...\"\"\n");
    printf("  Interpolation:  pr \"Value: /#x\"\n");
    printf("  Output:         pr \"message\", 123\n");
    printf("  Input:          inp.int \"prompt\", inp.str \"prompt\"\n");
    printf("  While Loop:     lp !!condition!! :: ... ::\n");
    printf("  For Loop:       fr !!i in 1@100!! :: ... ::\n");
    printf("  Conditionals:   if !!condition!! :: ... :: elif ... :: el :: ... ::\n");
    printf("  Functions:      fn name !!<x:9, u>>!! :: rt x ::\n");
}

static char* read_file_contents(const char* filepath) {
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "[Yalu Error] Cannot open file '%s'\n", filepath);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fclose(f);
        fprintf(stderr, "[Yalu Error] Cannot determine size of '%s'\n", filepath);
        return NULL;
    }

    char* buffer = (char*)yalu_malloc((size_t)size + 1);
    size_t read_bytes = fread(buffer, 1, (size_t)size, f);
    buffer[read_bytes] = '\0';
    fclose(f);

    /* Strip UTF-8 BOM if present */
    if ((unsigned char)buffer[0] == 0xEF &&
        (unsigned char)buffer[1] == 0xBB &&
        (unsigned char)buffer[2] == 0xBF) {
        memmove(buffer, buffer + 3, read_bytes - 3 + 1);
    }

    return buffer;
}

static int run_file(const char* filepath) {
    char* source = read_file_contents(filepath);
    if (!source) return 1;

    yalu_errors_reset();
    yalu_set_source_buffer(source);

    Lexer lexer;
    lexer_init(&lexer, filepath, source);

    Parser parser;
    parser_init(&parser, &lexer, filepath);

    ASTNode* program = parser_parse(&parser);
    if (!program || g_yalu_error_count > 0) {
        ast_free(program);
        yalu_free(source);
        return 1;
    }

    Interpreter interp;
    interp_init(&interp);
    interp_set_current_filepath(&interp, filepath);

    bool ok = interp_interpret(&interp, program);

    interp_free(&interp);
    ast_free(program);
    yalu_free(source);

    return ok ? 0 : 1;
}

static int check_file(const char* filepath) {
    char* source = read_file_contents(filepath);
    if (!source) return 1;

    yalu_errors_reset();
    yalu_set_source_buffer(source);

    Lexer lexer;
    lexer_init(&lexer, filepath, source);

    Parser parser;
    parser_init(&parser, &lexer, filepath);

    ASTNode* program = parser_parse(&parser);

    int exit_code = (program && g_yalu_error_count == 0) ? 0 : 1;
    if (exit_code == 0) {
        printf("[Yalu Check] '%s' parsed successfully with 0 errors.\n", filepath);
    } else {
        fprintf(stderr, "[Yalu Check] '%s' failed validation with %d error(s).\n", filepath, g_yalu_error_count);
    }

    ast_free(program);
    yalu_free(source);
    return exit_code;
}

static void run_repl(void) {
    print_version();
    printf("Type 'exit' or 'quit' to exit REPL.\n\n");

    Interpreter interp;
    interp_init(&interp);
    interp_set_current_filepath(&interp, "<repl>");

    char line_buf[2048];
    while (true) {
        printf("yl> ");
        fflush(stdout);

        if (!fgets(line_buf, sizeof(line_buf), stdin)) {
            printf("\n");
            break;
        }

        /* Check exit */
        if (strncmp(line_buf, "exit", 4) == 0 || strncmp(line_buf, "quit", 4) == 0) {
            break;
        }

        /* Check for multiline block */
        size_t len = strlen(line_buf);
        if (len > 0 && line_buf[len - 1] == '\n') line_buf[len - 1] = '\0';
        if (strlen(line_buf) == 0) continue;

        yalu_errors_reset();
        yalu_set_source_buffer(line_buf);

        Lexer lexer;
        lexer_init(&lexer, "<repl>", line_buf);

        Parser parser;
        parser_init(&parser, &lexer, "<repl>");

        ASTNode* program = parser_parse(&parser);
        if (program && g_yalu_error_count == 0) {
            interp_interpret(&interp, program);
        }

        ast_free(program);
    }

    interp_free(&interp);
}

#ifdef _WIN32
typedef void* HWND;
typedef void* HICON;
typedef void* HINSTANCE;
typedef void* HMODULE;
typedef intptr_t LPARAM;
typedef uintptr_t WPARAM;
typedef unsigned int UINT;
__declspec(dllimport) HWND __stdcall GetConsoleWindow(void);
__declspec(dllimport) HMODULE __stdcall GetModuleHandleA(const char* lpModuleName);
__declspec(dllimport) HICON __stdcall LoadIconA(HINSTANCE hInstance, const char* lpIconName);
__declspec(dllimport) intptr_t __stdcall SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
#define WM_SETICON 0x0080
#define ICON_SMALL 0
#define ICON_BIG 1
#define MAKEINTRESOURCEA(i) ((char*)((uintptr_t)((uint16_t)(i))))
static void init_windows_app_icon(void) {
    HWND hwnd = GetConsoleWindow();
    if (hwnd) {
        HICON hIcon = LoadIconA(GetModuleHandleA(NULL), MAKEINTRESOURCEA(1));
        if (hIcon) {
            SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        }
    }
}
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    init_windows_app_icon();
#endif
    if (argc < 2) {
        run_repl();
        return 0;
    }

    const char* arg1 = argv[1];

    if (strcmp(arg1, "--version") == 0 || strcmp(arg1, "-v") == 0) {
        print_version();
        return 0;
    }

    if (strcmp(arg1, "--help") == 0 || strcmp(arg1, "-h") == 0) {
        print_help();
        return 0;
    }

    if (strcmp(arg1, "check") == 0) {
        if (argc < 3) {
            fprintf(stderr, "[Yalu Error] Missing filename for 'check'. Usage: yalu.exe check <file.yl>\n");
            return 1;
        }
        return check_file(argv[2]);
    }

    if (strcmp(arg1, "run") == 0) {
        if (argc < 3) {
            fprintf(stderr, "[Yalu Error] Missing filename for 'run'. Usage: yalu.exe run <file.yl>\n");
            return 1;
        }
        return run_file(argv[2]);
    }

    /* Direct file execution: yalu.exe main.yl */
    return run_file(arg1);
}

