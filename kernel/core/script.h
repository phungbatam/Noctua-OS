#ifndef NOCTUA_SCRIPT_H
#define NOCTUA_SCRIPT_H

#define MAX_VARS      64
#define VAR_NAME_MAX  32
#define VAR_VAL_MAX   128
#define SCRIPT_STACK  16

typedef struct {
    char name[VAR_NAME_MAX];
    char value[VAR_VAL_MAX];
} script_var_t;

typedef struct {
    script_var_t vars[MAX_VARS];
    int var_count;
    int if_depth;
    int if_skip[SCRIPT_STACK];
} script_env_t;

void script_init(void);
int  script_set_var(const char *name, const char *value);
const char *script_get_var(const char *name);
int  script_expand(const char *input, char *output, int max_len);
int  script_run_file(const char *path);
int  script_run_line(const char *line, script_env_t *env);

#endif
