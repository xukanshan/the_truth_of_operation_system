#ifndef __SHELL_BUILDIN_CMD_H
#define __SHELL_BUILDIN_CMD_H
#include "global.h"
void make_clear_abs_path(char *path, char *final_path);
void buildin_pwd(uint32_t argc, char **argv UNUSED);
char *buildin_cd(uint32_t argc, char **argv);
void buildin_ls(uint32_t argc, char **argv);
void buildin_ps(uint32_t argc, char **argv UNUSED);
void buildin_clear(uint32_t argc, char **argv UNUSED);
int32_t buildin_mkdir(uint32_t argc, char **argv);
int32_t buildin_rmdir(uint32_t argc, char **argv);
int32_t buildin_rm(uint32_t argc, char **argv);
#endif