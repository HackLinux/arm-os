#ifndef _COMMAND_H_INCLUDED_
#define _COMMAND_H_INCLUDED_

#include "include/kernel/defines.h"

/*! echoコマンド */
void echo_command(char buf[]);

/*! helpコマンド */
void help_command(char *buf);

/*! sendlogコマンド */
void sendlog_command(void);

/*! recvlogコマンド */
void recvlog_command(void);

/* dumpコマンド */
void dump_command(char *buf);

/*! fatloadコマンド */
void fatload_command(char *buf);

/*! fatload制御 */
int do_fat_fsload(int argc, char *argv[]);

/*! fatlsコマンド */
void fatls_command(char *buf);

/*! fatls制御 */
int do_fat_ls (int argc, char *argv[]);

/*! fatinfoコマンド */
void fatinfo_command(char *buf);

/*! fsinfo制御 */
int do_fat_fsinfo (int argc, char *argv[]);

/*! tsetloadコマンド */
void tsetload_command(char *buf);

/*! goコマンド */
void go_command(char *buf);


#endif
