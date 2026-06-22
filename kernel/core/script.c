#include "script.h"
#include "screen.h"
#include "string.h"
#include "string.h"
#include "fs/fat32.h"
#include "mm/heap.h"

static script_env_t global_env;

void script_init(void) {
    memset(&global_env, 0, sizeof(global_env));
    script_set_var("?", "0");
    script_set_var("OS", "Noctua OS");
    script_set_var("SHELL", "noctua-sh");
    screen_term_write("[SCRIPT] Scripting engine initialized\n");
}

int script_set_var(const char *name, const char *value) {
    if (!name || !value) return -1;

    for (int i = 0; i < global_env.var_count; i++) {
        if (strcmp(global_env.vars[i].name, name) == 0) {
            strncpy(global_env.vars[i].value, value, VAR_VAL_MAX - 1);
            global_env.vars[i].value[VAR_VAL_MAX - 1] = 0;
            return 0;
        }
    }

    if (global_env.var_count >= MAX_VARS) return -1;

    strncpy(global_env.vars[global_env.var_count].name, name, VAR_NAME_MAX - 1);
    global_env.vars[global_env.var_count].name[VAR_NAME_MAX - 1] = 0;
    strncpy(global_env.vars[global_env.var_count].value, value, VAR_VAL_MAX - 1);
    global_env.vars[global_env.var_count].value[VAR_VAL_MAX - 1] = 0;
    global_env.var_count++;
    return 0;
}

const char *script_get_var(const char *name) {
    for (int i = 0; i < global_env.var_count; i++) {
        if (strcmp(global_env.vars[i].name, name) == 0) {
            return global_env.vars[i].value;
        }
    }
    return NULL;
}

int script_expand(const char *input, char *output, int max_len) {
    int out_pos = 0;

    for (int i = 0; input[i] && out_pos < max_len - 1; i++) {
        if (input[i] == '$' && input[i + 1] == '{') {
            i += 2;
            char vname[VAR_NAME_MAX];
            int vpos = 0;
            while (input[i] && input[i] != '}' && vpos < VAR_NAME_MAX - 1) {
                vname[vpos++] = input[i++];
            }
            vname[vpos] = 0;
            if (input[i] == '}') {
                const char *val = script_get_var(vname);
                if (val) {
                    int vlen = strlen(val);
                    for (int j = 0; j < vlen && out_pos < max_len - 1; j++) {
                        output[out_pos++] = val[j];
                    }
                }
            }
        } else if (input[i] == '$' && input[i + 1] == '?') {
            i++;
            const char *val = script_get_var("?");
            if (val) {
                int vlen = strlen(val);
                for (int j = 0; j < vlen && out_pos < max_len - 1; j++) {
                    output[out_pos++] = val[j];
                }
            }
        } else {
            output[out_pos++] = input[i];
        }
    }

    output[out_pos] = 0;
    return out_pos;
}

static void script_exec_cmd(const char *cmd, script_env_t *env) {
    (void)env;

    char expanded[256];
    script_expand(cmd, expanded, sizeof(expanded));

    extern void execute(const char *cmd);
    execute(expanded);
}

int script_run_line(const char *line, script_env_t *env) {
    while (*line == ' ' || *line == '\t') line++;
    if (!*line || *line == '#') return 0;

    char buf[256];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    char *p = buf;
    while (*p == ' ' || *p == '\t') p++;

    if (strncmp(p, "export ", 7) == 0) {
        char *eq = p + 7;
        char *val = NULL;
        while (*eq && *eq != '=') eq++;
        if (*eq == '=') {
            *eq = 0;
            val = eq + 1;
        }
        if (val) {
            script_set_var(p + 7, val);
        }
        return 0;
    }

    script_exec_cmd(p, env);
    return 0;
}

int script_run_file(const char *path) {
    file_handle_t *file = file_open(path, 0);
    if (!file) {
        screen_term_write("[SCRIPT] File not found: ");
        screen_term_write(path);
        screen_term_write("\n");
        return -1;
    }

    uint32_t size = file->node->size;
    char *content = (char *)kmalloc(size + 1);
    if (!content) {
        file_close(file);
        return -1;
    }

    file_read(file, content, size);
    content[size] = 0;

    script_env_t env;
    memset(&env, 0, sizeof(env));

    char *line_start = content;
    for (uint32_t i = 0; i <= size; i++) {
        if (content[i] == '\n' || content[i] == '\r' || content[i] == 0) {
            if (content[i] == '\r' && content[i + 1] == '\n') {
                content[i] = 0;
                i++;
            } else {
                content[i] = 0;
            }
            if (*line_start) {
                script_run_line(line_start, &env);
            }
            line_start = content + i + 1;
            if (content[i] == 0) break;
        }
    }

    kfree(content);
    file_close(file);
    return 0;
}
