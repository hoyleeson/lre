#ifndef _RULE_LRC_BUILTIN_LRC_H
#define _RULE_LRC_BUILTIN_LRC_H

int lrc_file_init(void);
int lrc_file_release(void);

int lrc_process_init(void);
int lrc_process_release(void);

int lrc_network_init(void);
int lrc_network_release(void);

int lrc_c_fuzzypath_init(void);
int lrc_c_fuzzypath_release(void);

int lrc_c_splicepath_init(void);
int lrc_c_splicepath_release(void);


int lrc_builtin_init(void);
void lrc_builtin_release(void);

#endif
