#
# CFLAGS
# -nostdinc         : ヘッダファイルのための標準システムディレクトリを検索しない
# -nostdlib         : 標準ライブラリ等を使用しない
# -fno-builtin      : ビルドイン関数の無効
# -fsigned-char     : char型を自動的にunsinged charとしない
# ABIオプション
# -mabi=apcs-gnu    : GNU ABIインターフェース
# imabi=aapcs-linux : Version ? EABIインターフェース
# なし              : Version ? EABIインターフェース
# アーキテクチャ
# -march            : 指定したCPUだけで動作するコードを生成 -mcpuより速いコード生成
# -mcpu             : 同系列のCPUでも動作するコード生成
# -mtune            : -mcpuの別名(gcc3.4以降)


PREFIX  = $(HOME)/bin/CodeSourcery/Sourcery_G++_Lite
BINDIR  = $(PREFIX)/bin


ARCH      = arm-none-eabi
ADDNAME   = $(ARCH)-


AR      = $(BINDIR)/$(ADDNAME)ar
AS      = $(BINDIR)/$(ADDNAME)as
CC      = $(BINDIR)/$(ADDNAME)gcc
LD      = $(BINDIR)/$(ADDNAME)ld
NM      = $(BINDIR)/$(ADDNAME)nm

OBJCOPY = $(BINDIR)/$(ADDNAME)objcopy
OBJDUMP = $(BINDIR)/$(ADDNAME)objdump
RANLIB  = $(BINDIR)/$(ADDNAME)ranlib
STRIP   = $(BINDIR)/$(ADDNAME)strip


TARGET = bin/uImage


#依存関係より，-oを加えない
CFLAGS = -Wall -nostdlib -fno-builtin -fsigned-char
CFLAGS += -march=armv7-a
CFLAGS += -I.
CFLAGS += -g# デバッガ対応(Jtag+openOCD+remoteGDB)
CFLAGS += -Os


CFLAGS += -DKERNEL
CFLAGS += -DTSK_LIBRARY
CFLAGS += -DLOG
#CFLAGS += -DDEBUG_LEVEL1 -DARCH_CPU_LIB_MAIN
CFLAGS += -DKERNEL_MSG
CFLAGS += -DCONFIG_MMC -DCONFIG_RELOC_FIXUP_WORKS -DCONFIG_DOS_PARTITION -D__LITTLE_ENDIAN
#CFLAGS += クロック入力?


LFLAGS = -static -T kernel/kernel.scr


#include directive
#下位のmkファイルへのパラメータ渡し
ASM_SOURCES :=
C_SOURCES :=
C++_SOURCE :=

ARCH_CPU_DIR := arch/cpu/
CPU_DIR_INCLUDE :=
include $(ARCH_CPU_DIR)build.mk

TARGET_DRIVER_DIR := target/driver/
DRIVER_DIR_INCLUDE :=
include $(TARGET_DRIVER_DIR)build.mk

ARCH_GCC_DIR := arch/gcc/
GCC_DIR_INCLUDE :=
include $(ARCH_GCC_DIR)build.mk

FS_FAT_DIR := fs/fat/
FAT_DIR_INCLUDE :=
include $(FS_FAT_DIR)build.mk

KERNEL_DIR := kernel/
KERNEL_DIR_INCLUDE :=
include $(KERNEL_DIR)build.mk

KERNEL_SVC_DIR := kernel_svc/
KERNEL_SVC_DIR_INCLUDE :=
include $(KERNEL_SVC_DIR)build.mk

NET_DIR := net/
NET_DIR_INCLUDE :=
include $(NET_DIR)build.mk

LIB_C_DIR := lib/c/
C_DIR_INCLUDE :=
include $(LIB_C_DIR)build.mk

LIB_C++_DIR := lib/c++/
C++_DIR_INCLUDE :=
include $(LIB_C++_DIR)build.mk

TSKLIB_DIR := tsk_lib/
TSK_LIB_DIR_INCLUDE :=
include $(TSKLIB_DIR)build.mk


ASM_OBJ := $(ASM_SOURCES:.S=.o)
C_OBJ := $(C_SOURCES:.c=.o)
C++_OBJ := $(C++_SOURCES:.cpp=.o)
OBJS += $(ASM_OBJ) $(C_OBJ) $(C++_OBJ)
OBJS := $(addprefix objs/,$(OBJS))

OS_INCLUDE += $(CPU_DIR_INCLUDE) $(DRIVER_DIR_INCLUDE) $(GCC_DIR_INCLUDE) $(FAT_DIR_INCLUDE) \
							$(KERNEL_DIR_INCLUDE) $(KERNEL_SVC_DIR_INCLUDE) $(NET_DIR_INCLUDE) $(C_DIR_INCLUDE) \
							$(C++_DIR_INCLUDE) $(TSK_LIB_DIR_INCLUDE)

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(CFLAGS) $(LFLAGS)


# startup.oとmain.oコンポーネントを先に生成
# -先に生成しないと，シンボルがずれ，エントリーアドレスから起動できなくなる
objs/%.o : $(ARCH_CPU_DIR)%.S
	$(CC) -c $(CFLAGS) $(CPU_DIR_INCLUDE) $< -o $@

objs/%.o : $(ARCH_CPU_DIR)%.c
	$(CC) -c $(CFLAGS) $(CPU_DIR_INCLUDE) $< -o $@

objs/%.o : $(TARGET_DRIVER_DIR)%.c
	$(CC) -c $(CFLAGS) $(DRIVER_DIR_INCLUDE) $< -o $@

objs/%.o : $(ARCH_GCC_DIR)%.S
	$(CC) -c $(CFLAGS) $< -o $@

objs/%.o : $(ARCH_GCC_DIR)%.c
	$(CC) -c $(CFLAGS) $< -o $@

objs/%.o : $(FS_FAT_DIR)%.c
	$(CC) -c $(CFLAGS) $(FAT_DIR_INCLUDE) $< -o $@

objs/%.o : $(KERNEL_DIR)%.c
	$(CC) -c $(CFLAGS) $(KERNEL_DIR_INCLUDE) $< -o $@

objs/%.o : $(KERNEL_SVC_DIR)%.c
	$(CC) -c $(CFLAGS) $(KERNEL_SVC_DIR_INCLUDE) $< -o $@

objs/%.o : $(NET_DIR)%.c
	$(CC) -c $(CFLAGS) $(NET_DIR_INCLUDE) $< -o $@

objs/%.o : $(LIB_C_DIR)%.c
	$(CC) -c $(CFLAGS) $(C_DIR_INCLUDE) $< -o $@

objs/%.o : $(LIB_C++_DIR)%.c
	$(CC) -c $(CFLAGS) $(C++_DIR_INCLUDE) $< -o $@

objs/%.o : $(TSKLIB_DIR)%.c
	$(CC) -c $(CFLAGS) $(TSK_LIB_DIR_INCLUDE) $< -o $@


bin : $(TARGET)
	$(OBJCOPY) -O binary $(TARGET) $(TARGET).bin

exe :
	cp bin/uImage.bin ~
	sudo chmod 777 /dev/ttyUSB0
	minicom -o


#make imageを使用する時は，以下にu-bootのmkimageバイナリファイルをおく
image : $(TARGET).bin
	../u-boot/tools/mkimage \
	-A arm -O linux -T kernel -C none \
	-n "Angstrom/2.6.32/beagleboard" -a 0x80008000 -e 0x80008000 \
	-d $(TARGET).bin $(TARGET)


clean :
	rm -f $(OBJS) $(TARGET) $(TARGET).bin $(TARGET)~ $(TARGET).bin~
	rm -f *~ $(ARCH_CPU_DIR)*.*~ $(TARGET_DRIVER_DIR)*.*~ $(ARCH_GCC_DIR)*.*~ $(FS_FAT_DIR)*.*~ \
	$(KERNEL_DIR)*.*~ $(KERNEL_SVC_DIR)*.*~ $(NET_DIR)*.*~ $(LIB_C_DIR)*.*~ $(LIB_C++_DIR)*.*~ \
	$(TSKLIB)*.*~ include/c/*.*~ include/c++/*.*~ include/cpu/*.*~ include/driver/*.*~ include/fs/*.*~ \
	include/gcc/*.*~ include/kernel/*.*~ include/kernel_svc/*.*~ include/net/*.*~
