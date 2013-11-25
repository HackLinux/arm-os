#include "defines.h"
#include "command.h"
#include "kernel.h"
#include "syscall.h"
#include "net/xmodem.h"
#include "kernel_svc/log_manage.h"
#include "target/driver/part.h"
#include "target/driver/fat_driver.h"


#ifdef TSK_LIBRARY

/*! tsk_set1の起動 */
static void tsk_set1_command(void);

/*! tsk_set2の起動 */
static void tsk_set2_command(void);

/*! tsk_set3の起動 */
static void tsk_set3_command(void);

#endif


/*! リンカのシンボルを参照 */
extern UINT32 _logbuffer_start;
extern UINT32 _tskbuffer_start;


/*!
 * echoコマンド
 * buf[] : 標準出力するバッファ
 */
void echo_command(char buf[])
{
  puts(buf + 5);
  puts("\n");
}


/*!
 * helpコマンド
 * *buf : 標準出力するバッファポインタ
 */
void help_command(char *buf)
{
  /* helpメッセージ */
  if (*buf == '\0') {
    puts("echo    - out text serial line.\n");
    puts("sendlog - send log file over serial line(xmodem mode)\n");
    puts("recvlog - receive log file over serial line(xmodem mode\n");
    puts("run     - run task sets.\n");
    puts("dump    - memory dump.\n");
  }
  /* echo helpメッセージ */
  else if (!strncmp(buf, " echo", 5)) {
    puts("echo - out text serial line\n\n");
    puts("Usage:\n");
    puts("echo [arg...]\n");
    puts("  out [arg...] serial line\n");
  }
  /* sendlog helpメッセージ */
  else if (!strncmp(buf, " sendlog", 8)) {
    puts("sendlog - send log file over serial line(xmodem mode)\n");
  }
  /* recvlog helpメッセージ */
  else if (!strncmp(buf, " recvlog", 8)) {
    puts("recvlog - recv log file over serial line(xmodem mode)\n");
  }
  /* dump helpメッセージ */
  else if (!strncmp(buf, " dump", 5)) {
    puts("dump - memory dump\n");
  }
#ifdef TSK_LIBRARY
  /* run helpメッセージ */
  else if (!strncmp(buf, " run", 4)) {
    /* run help */
    puts("run - run task sets.\n\n");
    puts("Usage:\n");
    puts("run <tsk_set>\n");
    puts("  start the <tsk_set> that is specified in the argument\n");
  }
  /* tsk_set3 helpメッセージ */
  else if (!strncmp(buf, " tsk_set3", 9)) {
    puts("tsk_set3 - This sample task sets the \"TASK SYNCHRONIZATION SYSTEMCALLS\"\n\n");
    puts("Using system call:\n");
    puts("  acre_tsk(), sta_tsk(), slp_tsk(), wup_tsk(), rel_wai()\n");
  }
  /* tsk_set2 helpメッセージ */
  else if (!strncmp(buf, " tsk_set2", 9)) {
    puts("tsk_set2 - This sample task sets the \"TASK MANAGE SYSTEMCALLS\"\n\n");
    puts("Using system call:\n");
    puts("  run_tsk(), get_pri(), chg_pri()\n");
  }
  /* tsk_set1 helpメッセージ */
  else if (!strncmp(buf, " tsk_set1", 9)) {
    puts("tsk_set1 - This sample task sets the \"TASK MANAGE SYSTEMCALLS\"\n\n");
    puts("Using system call:\n");
    puts("  acre_tsk(), sta_tsk(), del_tsk(), ext_tsk(), exd_tsk(), ter_tsk()\n");
  }
  /* tsk_set 一覧 */
  else if (!strncmp(buf, " tsk_set", 8)) {
    puts("tsk_set1 - This sample task sets the \"TASK MANAGE SYSTEMCALLS\"\n");
    puts("tsk_set2 - This sample task sets the \"TASK MANAGE SYSTEMCALLS\"\n");
    puts("tsk_set3 - This sample task sets the \"TASK SYNCHRONIZATION SYSTEMCALLS\"\n");
  }
#endif
  /* helpに存在しないコマンド */
  else {
    puts("help unknown.\n");
  }
}


/*! sendlogコマンド */
void sendlog_command(void)
{
  UINT32 *logbuf;
  logbuf = (UINT32 *)&_logbuffer_start;

  /* ログをxmodemで送信して正常の場合 */
  if (send_xmodem((UINT8 *)logbuf, *logbuf + 4)) { /* +4はログ管理機構の先頭番地(サイズ格納位置分) */
    puts("log to xmodem OK.\n");
  }
  /* エラーの場合 */
  else {
    puts("log to xmodem error.\n");
  }
}


/*! recvlogコマンド */
void recvlog_command(void)
{
  UINT8 *loadbuf;
  INT32 size;

  loadbuf = (UINT8 *)(&_tskbuffer_start);
  size = recv_xmodem(loadbuf);
  adjust_timing_xmodem(); /* 転送アプリが終了し端末アプリに制御が戻るまで待ち合わせる */
  if (size < 0) {
    puts("\nXMODEM receive error!\n");
  }
  else {
    puts("\nXMODEM receive succeed.\n");
  }
}


/* dumpコマンド */
int dump_command(void)
{
  INT32 i;
  INT32 size = 1024;
  unsigned char *buf = (unsigned char *)&_tskbuffer_start;

  if (size < 0) {
    puts("no data.\n");
    return -1;
  }
  for (i = 0; i < size; i++) {
    putxval(buf[i], 2);
    if ((i & 0xf) == 15) {
      puts("\n");
    }
    else {
      if ((i & 0xf) == 7) puts(" ");
      puts(" ");
    }
  }
  puts("\n");

  return 0;
}


/*!
 * fatloadコマンド
 * *buf : 起動するタスクセット名が格納されたバッファ
 */
void fatload_command(char *buf)
{
  unsigned char *argv[16];
  int rval = 1;
  int i, j;

  argv[0] = strtok((unsigned char *)buf, ' ');
  for (i = 1; NULL != (argv[i] = strtok(NULL, ' ')); i++) {
    ;
  }
  putxval(i, 0);
  puts(" i value.\n");

  for (j = 0; j < i; j++) {
    puts((char *)argv[j]);
    puts(" argv value.\n");
  }

  rval = do_fat_fsload(i, (char **)argv);

  if (0 == rval) {
    puts("fatload successed.\n");
  }
  else {
    puts("not fatload successed.\n");
  }
}


/*! fatload制御 */
int do_fat_fsload(int argc, char *argv[])
{
  long size;
  unsigned long offset;
  unsigned long count;
  block_dev_desc_t *dev_desc=NULL;
  int dev=0;
  int part=1;
  char *ep;

  if (argc < 5) {
    puts ("usage: fatload <interface> <dev[:part]> <addr> <filename> [bytes]\n");
    return 1;
  }
  dev = (int)simple_strtoul(argv[2], &ep, 16);
  putxval(dev, 0);
  puts(" dev valude.\n");
  dev_desc=get_dev(argv[1],dev);
  if (dev_desc==NULL) {
    puts ("\n** Invalid boot device **\n");
    return 1;
  }
  if (*ep) {
    if (*ep != ':') {
      puts ("\n** Invalid boot device, use `dev[:part]' **\n");
      return 1;
    }
    part = (int)simple_strtoul(++ep, NULL, 16);
  }
  putxval(part, 0);
  puts(" part value.\n");
  if (fat_register_device(dev_desc,part)!=0) {
    puts("\n** Unable to use ");
    puts(argv[1]);
    puts(" ");
    putxval(dev, 0);
    puts(":");
    putxval(part, 0);
    puts(" for fatload **\n");
    return 1;
  }
  offset = simple_strtoul (argv[3], NULL, 16);
  if (argc == 6)
    count = simple_strtoul (argv[5], NULL, 16);
  else
    count = 0;
  size = file_fat_read (argv[4], (unsigned char *) offset, count);
  if(size==-1) {
    puts("\n** Unable to read ");
    puts(argv[4]);
    puts(" from ");
    puts(argv[1]);
    puts(" ");
    putxval(dev, 0);
    puts(":");
    putxval(part, 0);
    puts(" **\n");
    return 1;
  }

  putxval(size, 0);
  puts(" bytes read\n");

  return 0;
}


/*!
 * fatlsコマンド
 * *buf : 起動するタスクセット名が格納されたバッファ
 */
void fatls_command(char *buf)
{
  unsigned char *argv[16];
  int i, j;

  argv[0] = strtok((unsigned char *)buf, ' ');
  for (i = 1; NULL != (argv[i] = strtok(NULL, ' ')); i++) {
    ;
  }
  putxval(i, 0);
  puts(" i value.\n");

  for (j = 0; j < i; j++) {
    puts((char *)argv[j]);
    puts(" argv value.\n");
  }

  do_fat_ls(i, (char **)argv);

}


/*! fatls制御 */
int do_fat_ls (int argc, char *argv[])
{
  char *filename = "/";
  int ret;
  int dev=0;
  int part=1;
  char *ep;
  block_dev_desc_t *dev_desc=NULL;

  if (argc < 3) {
    puts ("usage: fatls <interface> <dev[:part]> [directory]\n");
    return (0);
  }
  dev = (int)simple_strtoul (argv[2], &ep, 16);
  dev_desc=get_dev(argv[1],dev);
  if (dev_desc==NULL) {
    puts ("\n** Invalid boot device **\n");
    return 1;
  }
  if (*ep) {
    if (*ep != ':') {
      puts ("\n** Invalid boot device, use `dev[:part]' **\n");
      return 1;
    }
    part = (int)simple_strtoul(++ep, NULL, 16);
  }
  if (fat_register_device(dev_desc,part)!=0) {
    puts("\n** Unable to use ");
    puts(argv[1]);
    puts(" ");
    putxval(dev, 0);
    puts(":");
    putxval(part, 0);
    puts(" for fatls **\n");
    return 1;
  }
  if (argc == 4)
    ret = file_fat_ls (argv[3]);
  else
    ret = file_fat_ls (filename);

  if(ret!=0)
    puts("No Fat FS detected\n");
  
  return (ret);
}


/*!
 * fatinfoコマンド
 * *buf : 起動するタスクセット名が格納されたバッファ
 */
void fatinfo_command(char *buf)
{
  unsigned char *argv[16];
  int i, j;

  argv[0] = strtok((unsigned char *)buf, ' ');
  for (i = 1; NULL != (argv[i] = strtok(NULL, ' ')); i++) {
    ;
  }
  putxval(i, 0);
  puts(" i value.\n");

  for (j = 0; j < i; j++) {
    puts((char *)argv[j]);
    puts(" argv value.\n");
  }

  do_fat_fsinfo(i, (char **)argv);

}


/* fsinfo制御 */
int do_fat_fsinfo (int argc, char *argv[])
{
  int dev=0;
  int part=1;
  char *ep;
  block_dev_desc_t *dev_desc=NULL;

  if (argc < 2) {
    puts ("usage: fatinfo <interface> <dev[:part]>\n");
    return (0);
  }
  dev = (int)simple_strtoul (argv[2], &ep, 16);
  dev_desc=get_dev(argv[1],dev);
  if (dev_desc==NULL) {
    puts ("\n** Invalid boot device **\n");
    return 1;
  }
  if (*ep) {
    if (*ep != ':') {
      puts ("\n** Invalid boot device, use `dev[:part]' **\n");
      return 1;
    }
    part = (int)simple_strtoul(++ep, NULL, 16);
  }
  if (fat_register_device(dev_desc,part)!=0) {
    puts("\n** Unable to use ");
    puts(argv[1]);
    puts(" ");
    putxval(dev, 0);
    puts(":");
    putxval(part, 0);
    puts(" for fatinfo **\n");
    return 1;
  }
  return (file_fat_detectfs ());
}


#ifdef TSK_LIBRARY

/*!
 * runコマンド
 * *buf : 起動するタスクセット名が格納されたバッファ
 */
void run_command(char *buf)
{
  if (!strncmp(buf, " tsk_set3", 10)) {
    tsk_set3_command();
  }
  else if (!strncmp(buf, " tsk_set2", 10)) {
    tsk_set2_command();
  }
  else if (!strncmp(buf, " tsk_set1", 10)) {
    tsk_set1_command();
  }
  else {
    puts("tsk_set unknown.\n");
  }
}


/*! tsk_set1の起動 */
static void tsk_set1_command(void)
{
  SYSCALL_PARAMCB tsk1_param;

  tsk1_param.un.acre_tsk.func = sample_tsk1_main;
  tsk1_param.un.acre_tsk.name = "sample_tsk1";
  tsk1_param.un.acre_tsk.priority = 5;
  tsk1_param.un.acre_tsk.stacksize = 0x100;
  tsk1_param.un.acre_tsk.argc = 0;
  tsk1_param.un.acre_tsk.argv = NULL;

  sample_tsk3_id = mz_iacre_tsk(&tsk1_param);
  mz_ista_tsk(sample_tsk3_id);
}


/*! tsk_set2の起動 */
static void tsk_set2_command(void)
{
  SYSCALL_PARAMCB tsk1_param;

  tsk1_param.un.acre_tsk.func = sample_tsk4_main;
  tsk1_param.un.acre_tsk.name = "sample_tsk4";
  tsk1_param.un.acre_tsk.priority = 3;
  tsk1_param.un.acre_tsk.stacksize = 0x100;
  tsk1_param.un.acre_tsk.argc = 0;
  tsk1_param.un.acre_tsk.argv = NULL;

  sample_tsk4_id = mz_iacre_tsk(&tsk1_param);
  mz_ista_tsk(sample_tsk4_id);
}


/*! tsk_set3の起動 */
static void tsk_set3_command(void)
{
  SYSCALL_PARAMCB tsk1_param;

  tsk1_param.un.acre_tsk.func = sample_tsk6_main;
  tsk1_param.un.acre_tsk.name = "sample_tsk6";
  tsk1_param.un.acre_tsk.priority = 5;
  tsk1_param.un.acre_tsk.stacksize = 0x100;
  tsk1_param.un.acre_tsk.argc = 0;
  tsk1_param.un.acre_tsk.argv = NULL;

  sample_tsk6_id = mz_iacre_tsk(&tsk1_param);
  mz_ista_tsk(sample_tsk6_id);
}

#endif
