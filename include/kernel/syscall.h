/*
 * ・syscall.hはシステムコールパラメータやリターンパラメータを設定するsyscall.cの
 *   ヘッダファイルである.
 * ・システムコール番号，システムコールのパラメータ，リターンパラメータの
 *   グローバルエリアの順に定義がされている(システムコールはトラップを発行するので，
 *   必ずパラメータを
 *   逃しておくグローバルエリアが必要となる).
 * ・また，サービスコールはシステムコールと同じグローバルエリアを使用するものとする.
 * ・システムコール番号とパラメータ情報はポインタとしてTCBが持っている.
 * ・また，同時にシステムコールが2以上呼び出されることはないので，unionで領域節約を
 *   行っている.                                                                *
 */



#ifndef _SYSCALL_H_INCLUDED_
#define _SYSCALL_H_INCLUDED_

#include "include/kernel/defines.h"
//#include "interrupt.h"


#define WAIT_ERCD_NOCHANGE 0 /*! 他のシステムコールにより返却値が書き換えられない場合のマクロ(TCBのwait_info_wait_erに設定) */


/*! システムコール番号(ISR)の定義 */
typedef enum {
  ISR_TYPE_ACRE_TSK = 0,  /*! タスク生成  */
  ISR_TYPE_DEL_TSK,       /*! タスク排除  */
  ISR_TYPE_STA_TSK,       /*! タスク起動  */
  ISR_TYPE_RUN_TSK,       /*! タスクの生成と起動  */
  ISR_TYPE_EXT_TSK,       /*! タスクの終了  */
  ISR_TYPE_EXD_TSK,       /*! タスクの終了と排除  */
  ISR_TYPE_TER_TSK,       /*! タスク強制終了  */
  ISR_TYPE_GET_PRI,       /*! タスク優先度取得  */
  ISR_TYPE_CHG_PRI,       /*! タスク優先度変更  */
  ISR_TYPE_SLP_TSK,       /*! 自タスク起床待ち(スリープ) */
  ISR_TYPE_WUP_TSK,       /*! タスクの起床(ウェイクアップ) */
  ISR_TYPE_REL_WAI,       /*! タスク待ち状態強制解除  */
  ISR_TYPE_GET_MPF,       /*! 固定長メモリブロックの獲得 */
  ISR_TYPE_REL_MPF,       /*! 固定長メモリブロックの解放 */
  ISR_TYPE_DEF_INH,       /*! 割込みハンドラ登録 */
  ISR_TYPE_ENA_DSP,       /*! ディスパッチの許可 */
  ISR_TYPE_SEL_SCHDUL,    /*! タスクスケジューラ動的切り替え サービスコールのみとなるので，実際はいらないが，他と一貫性と保つため */
  ISR_NUM,                /*! ISRの数 */
} ISR_TYPE;


typedef enum {
  ISR_TYPE_IACRE_TSK = 0, /*! タスク生成  */
  ISR_TYPE_ISTA_TSK,      /*! タスク起動  */
  ISR_INUM,
} ISR_ITYPE;


/*! システムコール呼び出し時のパラメータ&リターンパラメータ退避領域 */
typedef struct {
  union {
    /*! タスクの生成 */
    struct {
      TSK_FUNC func;
      char *name;
      int priority;
      int stacksize;
      int rate;
      int rel_exetim;
      int deadtim;
      int floatim;
      int argc;
      char **argv;
      ER_ID ret;
    } acre_tsk;
    /*! タスクの排除  */
    struct {
      ER_ID tskid;
      ER ret;
    } del_tsk;
    /*! タスクの起動 */
    struct {
      ER_ID tskid;
      ER ret;
    } sta_tsk;
    /*! タスクの生成と起動  */
    struct {
      TSK_FUNC func;
      char *name;
      int priority;
      int stacksize;
      int rate;
      int rel_exetim;
      int deadtim;
      int floatim;
      int argc;
      char **argv;
      ER_ID ret;
    } run_tsk;
    /*! タスクの終了  */
    struct {
      int dmummy;
    } ext_tsk;
    /*! タスクの終了と排除  */
    struct {
      int dummy;
    } exd_tsk;
    /*! タスクの強制解除  */
    struct {
      ER_ID tskid;
      ER ret;
    } ter_tsk;
    /*! タスクの優先度取得 */
    struct {
      ER_ID tskid;
      int *p_tskpri;
      ER ret;
    } get_pri;
    /*! タスクの優先度変更  */
    struct {
      ER_ID tskid;
      int tskpri;
      ER ret;
    } chg_pri;
    /*! タスクの起床待ち  */
    struct {
      ER ret;
    } slp_tsk;
    /*! タスクの起床  */
    struct {
      ER_ID tskid;
      ER ret;
    } wup_tsk;
    /*! タスク待ち状態強制解除 */
    struct {
      ER_ID tskid;
      ER ret;
    } rel_wai;
    /*! 固定長メモリブロック取得 */
    struct {
      int size;
      void *ret;
    } get_mpf;
    /*! 固定長メモリブロック解放 */
    struct {
      char *p;
      int ret;
    } rel_mpf;
    /*! 割込みハンドラの登録 */
    struct {
      INTRPT_TYPE type;
      IR_HANDL handler;
      ER ret;
    } def_inh;
    /*! タスクスケジューラ動的切り替え */
    /* サービスコールのみとなるので，実際はいらないが，他と一貫性と保つため */
    struct {
      SCHDUL_TYPE type;
      long param;
      ER ret;
    } sel_schdul;
  } un;
} SYSCALL_PARAMCB;


/* システムコール */
/*! mz_acre_tsk():タスクコントロールブロックの生成(ID自動割付) */
ER_ID mz_acre_tsk(SYSCALL_PARAMCB *par);

/*! mz_del_tsk():スレッドの排除 */
ER mz_del_tsk(ER_ID tskid);

/*! mz_sta_tsk():スレッドの起動 */
ER mz_sta_tsk(ER_ID tskid);

/*! mz_run_tsk():スレッドの生成(ID自動割付)と起動 */
ER_ID mz_run_tsk(SYSCALL_PARAMCB *par);

/*! mz_ext_tsk():自タスクの終了 */
void mz_ext_tsk(void);

/*! mz_exd_tsk():自スレッドの終了と排除 */
void mz_exd_tsk(void);

/*! mz_ter_tsk():スレッドの強制終了 */
ER mz_ter_tsk(ER_ID tskid);

/*! mz_get_pri():スレッドの優先度取得 */
ER mz_get_pri(ER_ID tskid, int *p_tskpri);

/*! mz_chg_pri():スレッドの優先度変更 */
ER mz_chg_pri(ER_ID tskid, int tskpri);

/*! chg_slt():タスクタイムスライスの変更 */
ER mz_chg_slt(SCHDUL_TYPE type, ER_ID tskid, int slice);

/*! get_slt():タスクタイムスライスの取得 */
ER mz_get_slt(SCHDUL_TYPE type, ER_ID tskid, int *p_slice);

/*! mz_slp_tsk():自タスクの起床待ち */
ER mz_slp_tsk(void);

/*! mz_wup_tsk():タスクの起床 */
ER mz_wup_tsk(ER_ID tskid);

/*! mz_rel_wai():待ち状態強制解除 */
ER mz_rel_wai(ER_ID tskid);

/*! mz_get_mpf():動的メモリ獲得 */
void* mz_get_mpf(int size);

/*! mz_rel_mpf():動的メモリ解放 */
int mz_rel_mpf(void *p);

/*! mz_def_inh():割込みハンドラの定義 */
ER mz_def_inh(INTRPT_TYPE type, IR_HANDL handler);


#endif
