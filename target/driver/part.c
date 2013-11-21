#include "part.h"
#include "mmc.h"
#include "kernel/defines.h"

#if (defined(CONFIG_CMD_IDE) ||                 \
     defined(CONFIG_CMD_MG_DISK) ||             \
     defined(CONFIG_CMD_SATA) ||                \
     defined(CONFIG_CMD_SCSI) ||                \
     defined(CONFIG_CMD_USB) ||                 \
     defined(CONFIG_MMC) ||                     \
     defined(CONFIG_SYSTEMACE) )

struct block_drvr {
  char *name;
  block_dev_desc_t* (*get_dev)(int dev);
};

static const struct block_drvr block_drvr[] = {
#if defined(CONFIG_CMD_IDE)
  { .name = "ide", .get_dev = ide_get_dev, },
#endif
#if defined(CONFIG_CMD_SATA)
  {.name = "sata", .get_dev = sata_get_dev, },
#endif
#if defined(CONFIG_CMD_SCSI)
  { .name = "scsi", .get_dev = scsi_get_dev, },
#endif
#if defined(CONFIG_CMD_USB) && defined(CONFIG_USB_STORAGE)
  { .name = "usb", .get_dev = usb_stor_get_dev, },
#endif
#if defined(CONFIG_MMC)
  { .name = "mmc", .get_dev = mmc_get_dev, },
#endif
#if defined(CONFIG_SYSTEMACE)
  { .name = "ace", .get_dev = systemace_get_dev, },
#endif
#if defined(CONFIG_CMD_MG_DISK)
  { .name = "mgd", .get_dev = mg_disk_get_dev, },
#endif
  { },
};


//DECLARE_GLOBAL_DATA_PTR;

block_dev_desc_t *get_dev(char* ifname, int dev)
{
  const struct block_drvr *drvr = block_drvr;
  block_dev_desc_t* (*reloc_get_dev)(int dev);

  while (drvr->name) {
    reloc_get_dev = drvr->get_dev;
#ifndef CONFIG_RELOC_FIXUP_WORKS
    reloc_get_dev += gd->reloc_off;
#endif
    if (strncmp(ifname, drvr->name, strlen(drvr->name)) == 0)
      return reloc_get_dev(dev);
    drvr++;
  }
  return NULL;
}
#else
block_dev_desc_t *get_dev(char* ifname, int dev)
{
  return NULL;
}
#endif


int get_partition_info (block_dev_desc_t *dev_desc, int part
					, disk_partition_t *info)
{
	switch (dev_desc->part_type) {
#ifdef CONFIG_MAC_PARTITION
	case PART_TYPE_MAC:
		if (get_partition_info_mac(dev_desc,part,info) == 0) {
#if 0
			PRINTF ("## Valid MAC partition found ##\n");
#endif
      puts("## Valid MAC partition found ##\n");
			return (0);
		}
		break;
#endif

#ifdef CONFIG_DOS_PARTITION
	case PART_TYPE_DOS:
		if (get_partition_info_dos(dev_desc,part,info) == 0) {
#if 0
			PRINTF ("## Valid DOS partition found ##\n"w);
#endif
      puts("## Valid DOS partition found ##\n");
			return (0);
		}
		break;
#endif

#ifdef CONFIG_ISO_PARTITION
	case PART_TYPE_ISO:
		if (get_partition_info_iso(dev_desc,part,info) == 0) {
#if 0
			PRINTF ("## Valid ISO boot partition found ##\n");
#endif
      puts("## Valid ISO boot partition found ##\n");
			return (0);
		}
		break;
#endif

#ifdef CONFIG_AMIGA_PARTITION
	case PART_TYPE_AMIGA:
	    if (get_partition_info_amiga(dev_desc, part, info) == 0)
	    {
#if 0
		PRINTF ("## Valid Amiga partition found ##\n");
#endif
    puts("## Valid Amiga partition found ##\n");
		return (0);
	    }
	    break;
#endif

#ifdef CONFIG_EFI_PARTITION
	case PART_TYPE_EFI:
		if (get_partition_info_efi(dev_desc,part,info) == 0) {
#if 0
			PRINTF ("## Valid EFI partition found ##\n");
#endif
      puts("## Valid EFI partition found ##\n");
			return (0);
		}
		break;
#endif
	default:
		break;
	}
	return (-1);
}


#if (defined(CONFIG_CMD_IDE) || \
     defined(CONFIG_CMD_MG_DISK) || \
     defined(CONFIG_CMD_SATA) || \
     defined(CONFIG_CMD_SCSI) || \
     defined(CONFIG_CMD_USB) || \
     defined(CONFIG_MMC) || \
     defined(CONFIG_SYSTEMACE) )

/* ------------------------------------------------------------------------- */
/*
 * reports device info to the user
 */
void dev_print (block_dev_desc_t *dev_desc)
{
#ifdef CONFIG_LBA48
	uint64_t lba512; /* number of blocks if 512bytes block size */
#else
	lbaint_t lba512;
#endif

	if (dev_desc->type == DEV_TYPE_UNKNOWN) {
		puts ("not available\n");
		return;
	}

	switch (dev_desc->if_type) {
	case IF_TYPE_SCSI:
    puts("(");
    putxval(dev_desc->target, 0);
    puts(":");
    putxval(dev_desc->lun, 0);
    puts(") Vendor: ");
    puts(dev_desc->vendor);
    puts(" Prod.: ");
    puts(dev_desc->product);
    puts(" Rev: ");
    puts(dev_desc->revision);
    puts("\n");
		break;
	case IF_TYPE_ATAPI:
	case IF_TYPE_IDE:
	case IF_TYPE_SATA:
    puts("Model: ");
    puts(dev_desc->vendor);
    puts(" Firm: ");
    puts(dev_desc->revision);
    puts(" Ser#: ");
    puts(dev_desc->product);
    puts("\n");
		break;
	case IF_TYPE_SD:
	case IF_TYPE_MMC:
	case IF_TYPE_USB:
    puts("Vendor: ");
    puts(dev_desc->vendor);
    puts(" Rev: ");
    puts(dev_desc->revision);
    puts(" Prod: ");
    puts(dev_desc->product);
    puts("\n");
		break;
	case IF_TYPE_DOC:
		puts("device type DOC\n");
		return;
	case IF_TYPE_UNKNOWN:
		puts("device type unknown\n");
		return;
	default:
    puts("Unhandled device type: ");
    putxval(dev_desc->if_type, 0);
		return;
	}
	puts ("            Type: ");
	if (dev_desc->removable)
		puts ("Removable ");
	switch (dev_desc->type & 0x1F) {
	case DEV_TYPE_HARDDISK:
		puts ("Hard Disk");
		break;
	case DEV_TYPE_CDROM:
		puts ("CD ROM");
		break;
	case DEV_TYPE_OPDISK:
		puts ("Optical Device");
		break;
	case DEV_TYPE_TAPE:
		puts ("Tape");
		break;
	default:
    puts("# ");
    putxval((dev_desc->type & 0x1F), 0);
    puts(" #");
		break;
	}
	puts ("\n");
	if ((dev_desc->lba * dev_desc->blksz)>0L) {
		unsigned long mb, mb_quot, mb_rem, gb, gb_quot, gb_rem;
		lbaint_t lba;

		lba = dev_desc->lba;

		lba512 = (lba * (dev_desc->blksz/512));
		mb = (10 * lba512) / 2048;	/* 2048 = (1024 * 1024) / 512 MB */
		/* round to 1 digit */
		mb_quot	= mb / 10;
		mb_rem	= mb - (10 * mb_quot);

		gb = mb / 1024;
		gb_quot	= gb / 10;
		gb_rem	= gb - (10 * gb_quot);
#ifdef CONFIG_LBA48
		if (dev_desc->lba48)
      puts("            Supports 48-bit addressing\n");
#endif
#if defined(CONFIG_SYS_64BIT_LBA)
    puts("            Capacity: ");
    putxval(mb_quot, 0);
    puts(".");
    putxval(mb_rem, 0);
    puts(" MB = ");
    putxval(gb_quot, 0);
    puts(".");
    putxval(gb_rem, 0);
    puts(" GB (");
    putxval(lba, 0);
    puts(" x ");
    putxval(dev_desc->blksz, 0);
    puts(")\n");
#else
    puts("            Capacity: ");
    putxval(mb_quot, 0);
    puts(".");
    putxval(mb_rem, 0);
    puts(" MB = ");
    putxval(gb_quot, 0);
    puts(".");
    putxval(gb_rem, 0);
    puts(" GB (");
    putxval((unsigned long)lba, 0);
    puts(" x ");
    putxval(dev_desc->blksz, 0);
    puts(")\n");
#endif
	} else {
		puts ("            Capacity: not available\n");
	}
}
#endif