/* BSSセクションの初期化より前に変数を使用しない */
#include "kernel/defines.h"
#include "kernel/kernel.h"
#include "c_lib/lib.h"
#include "target/driver/serial_driver.h"
#include "arch/cpu/intr.h"
#include "kernel/command.h"


void uart_handler(void);
extern unsigned long _bss_start, _end;
extern unsigned long _tskbuffer_start, _tskbuffer_end;

/*! 資源ID */
ER_ID idle_id;
#ifdef TSK_LIBRARY
ER_ID sample_tsk1_id;
ER_ID sample_tsk2_id;
ER_ID sample_tsk3_id;
ER_ID sample_tsk4_id;
ER_ID sample_tsk5_id;
ER_ID sample_tsk6_id;
ER_ID sample_tsk7_id;
ER_ID sample_tsk8_id;
#endif

/* irqハンドラ */
void uart_handler(void)
{
  unsigned char c;
  static char buf[32];
  static int len;
  int it_type;

  /*
   * UART関連のレジスタであるIIRレジスタから割込みタイプの取得
   * (IIRレジスタは下位5ビットで割込みタイプを保持している)
   * シリアル受信割込み : 0x2
   * タイムアウト割込み(シリアル受信割込みを有効化すると同時に有効化される) : 0x6
   */
  it_type = (REG8_READ(UIIR) & 0x3E) >> 1;

  if (it_type == 2 || it_type == 6) {

    c = getc(); /* 受信FIFOからデータを読み出す事によって，割込み要因をクリア */

    if (c != '\n') {
      buf[len++] = c;
    }
    else {
      buf[len++] = '\0';
      /* echoコマンドの場合 */
      if (!strncmp(buf, "echo ", 5)) {
        echo_command(buf); /* echoコマンド(標準出力にテキストを出力する)呼び出し */
      }
      /* helpコマンドの場合 */
      else if (!strncmp(buf, "help", 4)) {
        help_command(&buf[4]); /* helpコマンド呼び出し */
      }
#ifdef TSK_LIBRARY
      /* runコマンドの場合 */
      else if (!strncmp(buf, "run", 3)) {
        run_command(&buf[3]); /* runコマンド(タスクセットの起動)呼び出し */
      }
#endif
      /* sendlogの場合 */
      else if (!strncmp(buf, "sendlog", 7)) {
        sendlog_command(); /* sendlogコマンド(xmodem送信モード)呼び出し */
      }
      /* recvlogの場合 */
      else if (!strncmp(buf, "recvlog", 7)) {
        recvlog_command(); /* recvlogコマンド(xmodem送信モード)呼び出し */
      }
      /* recvlogの場合 */
      else if (!strncmp(buf, "dump", 4)) {
        dump_command(); /* recvlogコマンド(xmodem送信モード)呼び出し */
      }
      /* 本システムに存在しないコマンド */
      else {
        puts("command unknown.\n");
      }
      puts("> ");
      len = 0;
    }
  }
  else {
    DEBUG_OUTMSG("not uart3 handler.\n");
  }
}


/*! OSメイン関数 */
/*  CPSRの外部割込み無効モードとして起動 */
int main(void)
{
  
  unsigned long *p;

  /* BSSセクションの初期化(BSSセクションの初期化はここでOK) */
  for (p = &_bss_start; p < &_end; p++) {
    *p = 0;
  }
  /* tskbuffuerセクションの初期化 */
  for (p = &_tskbuffer_start; p < &_tskbuffer_end; p++) {
    *p = 0;
  }

  uart3_init(); /* シリアルの初期化 */

  KERNEL_OUTMSG("kernel boot OK!\n");

  /* OSの動作開始 */
  kernel_init(start_threads, "init tsk", 0, 0x100, 0, NULL); /* initタスク起動 */
  
  /* 正常ならばここには戻ってこない */

  return 0;
}
