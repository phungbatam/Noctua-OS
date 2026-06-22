#ifndef TVN_INITD_H
#define TVN_INITD_H

void initd_init(void);
int initd_register_service(const char *name, int runlevel, void (*start)(void), void (*stop)(void));
int initd_start_service(const char *name);
int initd_stop_service(const char *name);
int initd_set_runlevel(int level);
int initd_get_runlevel(void);
void initd_list_services(void);
void initd_register_builtin_services(void);
void initd_main(void);

#endif
