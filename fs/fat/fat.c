/* private interface */

/* public interface */
/* include/fs */
#include "fat.h"
/* include/kernel */
#include "debug.h"
#include "defines.h"
/* include/driver */
#include "part.h"


/*
 * Convert a string to lowercase.
 */
static void
downcase(char *str)
{
  while (*str != '\0') {
    TOLOWER(*str);
    str++;
  }
}

static block_dev_desc_t *cur_dev = NULL;
static unsigned long part_offset = 0;
static int cur_part = 1;

#define DOS_PART_TBL_OFFSET 0x1be
#define DOS_PART_MAGIC_OFFSET 0x1fe
#define DOS_FS_TYPE_OFFSET  0x36

int disk_read (__u32 startblock, __u32 getsize, __u8 * bufptr)
{
  startblock += part_offset;
  if (cur_dev == NULL)
    return -1;
  if (cur_dev->block_read) {
    return cur_dev->block_read (cur_dev->dev
                                , startblock, getsize, (unsigned long *)bufptr);
  }
  return -1;
}


int
fat_register_device(block_dev_desc_t *dev_desc, int part_no)
{
  unsigned char buffer[SECTOR_SIZE];
  disk_partition_t info;

  if (!dev_desc->block_read) {
    return -1;
  }
  cur_dev = dev_desc;
  /* check if we have a MBR (on floppies we have only a PBR) */
  if (dev_desc->block_read (dev_desc->dev, 0, 1, (unsigned long *) buffer) != 1) {
    puts("** Can't read from device **\n");
    putxval(dev_desc->dev, 0);
    puts(" \n");
    return -1;
  }
  if (buffer[DOS_PART_MAGIC_OFFSET] != 0x55 ||
      buffer[DOS_PART_MAGIC_OFFSET + 1] != 0xaa) {
    /* no signature found */
    return -1;
  }
#if (defined(CONFIG_CMD_IDE) ||                 \
     defined(CONFIG_CMD_MG_DISK) ||             \
     defined(CONFIG_CMD_SATA) ||                \
     defined(CONFIG_CMD_SCSI) ||                \
     defined(CONFIG_CMD_USB) ||                 \
     defined(CONFIG_MMC) ||                     \
     defined(CONFIG_SYSTEMACE) )
  /* First we assume, there is a MBR */
  if (!get_partition_info (dev_desc, part_no, &info)) {
    part_offset = info.start;
    cur_part = part_no;
  } else if (!strncmp((char *)&buffer[DOS_FS_TYPE_OFFSET], "FAT", 3)) {
    /* ok, we assume we are on a PBR only */
    cur_part = 1;
    part_offset = 0;
  } else {
    puts("** Partition ");
    putxval(part_no, 0);
    puts(" not valid on device ");
    putxval(dev_desc->dev, 0);
    puts(" **\n");
    return -1;
  }

#else
  if (!strncmp((char *)&buffer[DOS_FS_TYPE_OFFSET],"FAT",3)) {
    /* ok, we assume we are on a PBR only */
    cur_part = 1;
    part_offset = 0;
    info.start = part_offset;
  } else {
    /* FIXME we need to determine the start block of the
     * partition where the DOS FS resides. This can be done
     * by using the get_partition_info routine. For this
     * purpose the libpart must be included.
     */
    part_offset = 32;
    cur_part = 1;
  }
#endif
  return 0;
}


/*
 * Get the first occurence of a directory delimiter ('/' or '\') in a string.
 * Return index into string if found, -1 otherwise.
 */
static int
dirdelim(char *str)
{
  char *start = str;

  while (*str != '\0') {
    if (ISDIRDELIM(*str)) return str - start;
    str++;
  }
  return -1;
}

/*
 * Extract zero terminated short name from a directory entry.
 */
static void get_name (dir_entry *dirent, char *s_name)
{
  char *ptr;

  memcpy (s_name, dirent->name, 8);
  s_name[8] = '\0';
  ptr = s_name;
  while (*ptr && *ptr != ' ')
    ptr++;
  if (dirent->ext[0] && dirent->ext[0] != ' ') {
    *ptr = '.';
    ptr++;
    memcpy (ptr, dirent->ext, 3);
    ptr[3] = '\0';
    while (*ptr && *ptr != ' ')
      ptr++;
  }
  *ptr = '\0';
  if (*s_name == DELETED_FLAG)
    *s_name = '\0';
  else if (*s_name == aRING)
    *s_name = DELETED_FLAG;
  downcase (s_name);
}

/*
 * Get the entry at index 'entry' in a FAT (12/16/32) table.
 * On failure 0x00 is returned.
 */
static __u32
get_fatent(fsdata *mydata, __u32 entry)
{
  __u32 bufnum;
  __u32 offset;
  __u32 ret = 0x00;

  switch (mydata->fatsize) {
  case 32:
    bufnum = entry / FAT32BUFSIZE;
    offset = entry - bufnum * FAT32BUFSIZE;
    break;
  case 16:
    bufnum = entry / FAT16BUFSIZE;
    offset = entry - bufnum * FAT16BUFSIZE;
    break;
  case 12:
    bufnum = entry / FAT12BUFSIZE;
    offset = entry - bufnum * FAT12BUFSIZE;
    break;

  default:
    /* Unsupported FAT size */
    return ret;
  }

  /* Read a new block of FAT entries into the cache. */
  if (bufnum != mydata->fatbufnum) {
    int getsize = FATBUFSIZE/FS_BLOCK_SIZE;
    __u8 *bufptr = mydata->fatbuf;
    __u32 fatlength = mydata->fatlength;
    __u32 startblock = bufnum * FATBUFBLOCKS;

    fatlength *= SECTOR_SIZE; /* We want it in bytes now */
    startblock += mydata->fat_sect; /* Offset from start of disk */

    if (getsize > fatlength) getsize = fatlength;
    if (disk_read(startblock, getsize, bufptr) < 0) {
#if 0
      FAT_DPRINT("Error reading FAT blocks\n");
#endif
      DEBUG_L1_FS_FAT_FAT_OUTMSG("Error reading FAT blocks\n");
      return ret;
    }
    mydata->fatbufnum = bufnum;
  }

  /* Get the actual entry from the table */
  switch (mydata->fatsize) {
  case 32:
    ret = FAT2CPU32(((__u32*)mydata->fatbuf)[offset]);
    break;
  case 16:
    ret = FAT2CPU16(((__u16*)mydata->fatbuf)[offset]);
    break;
  case 12: {
    __u32 off16 = (offset*3)/4;
    __u16 val1, val2;

    switch (offset & 0x3) {
    case 0:
      ret = FAT2CPU16(((__u16*)mydata->fatbuf)[off16]);
      ret &= 0xfff;
      break;
    case 1:
      val1 = FAT2CPU16(((__u16*)mydata->fatbuf)[off16]);
      val1 &= 0xf000;
      val2 = FAT2CPU16(((__u16*)mydata->fatbuf)[off16+1]);
      val2 &= 0x00ff;
      ret = (val2 << 4) | (val1 >> 12);
      break;
    case 2:
      val1 = FAT2CPU16(((__u16*)mydata->fatbuf)[off16]);
      val1 &= 0xff00;
      val2 = FAT2CPU16(((__u16*)mydata->fatbuf)[off16+1]);
      val2 &= 0x000f;
      ret = (val2 << 8) | (val1 >> 8);
      break;
    case 3:
      ret = FAT2CPU16(((__u16*)mydata->fatbuf)[off16]);;
      ret = (ret & 0xfff0) >> 4;
      break;
    default:
      break;
    }
  }
    break;
  }
#if 0
  FAT_DPRINT("ret: %d, offset: %d\n", ret, offset);
#endif
  DEBUG_L1_FS_FAT_FAT_OUTMSG("ret: ");
  DEBUG_L1_FS_FAT_FAT_OUTVLE(ret, 0);
  DEBUG_L1_FS_FAT_FAT_OUTMSG(", offset: ");
  DEBUG_L1_FS_FAT_FAT_OUTVLE(offset, 0);
  DEBUG_L1_FS_FAT_FAT_OUTMSG("\n");

  return ret;
}


/*
 * Read at most 'size' bytes from the specified cluster into 'buffer'.
 * Return 0 on success, -1 otherwise.
 */
static int
get_cluster(fsdata *mydata, __u32 clustnum, __u8 *buffer, unsigned long size)
{
  int idx = 0;
  __u32 startsect;

  if (clustnum > 0) {
    startsect = mydata->data_begin + clustnum*mydata->clust_size;
  } else {
    startsect = mydata->rootdir_sect;
  }

#if 0
  FAT_DPRINT("gc - clustnum: %d, startsect: %d\n", clustnum, startsect);
#endif
  DEBUG_L1_FS_FAT_FAT_OUTMSG("gc - clustnum: ");
  DEBUG_L1_FS_FAT_FAT_OUTVLE(clustnum, 0);
  DEBUG_L1_FS_FAT_FAT_OUTMSG(", startsect: ");
  DEBUG_L1_FS_FAT_FAT_OUTVLE(startsect, 0);
  DEBUG_L1_FS_FAT_FAT_OUTMSG("\n");
  if (disk_read(startsect, size/FS_BLOCK_SIZE , buffer) < 0) {
#if 0
    FAT_DPRINT("Error reading data\n");
#endif
    DEBUG_L1_FS_FAT_FAT_OUTMSG("Error reading data\n");
    return -1;
  }
  if(size % FS_BLOCK_SIZE) {
    __u8 tmpbuf[FS_BLOCK_SIZE];
    idx= size/FS_BLOCK_SIZE;
    if (disk_read(startsect + idx, 1, tmpbuf) < 0) {
#if 0
      FAT_DPRINT("Error reading data\n");
#endif
      DEBUG_L1_FS_FAT_FAT_OUTMSG("Error reading data\n");
      return -1;
    }
    buffer += idx*FS_BLOCK_SIZE;

    memcpy(buffer, tmpbuf, size % FS_BLOCK_SIZE);
    return 0;
  }

  return 0;
}


/*
 * Read at most 'maxsize' bytes from the file associated with 'dentptr'
 * into 'buffer'.
 * Return the number of bytes read or -1 on fatal errors.
 */
static long
get_contents(fsdata *mydata, dir_entry *dentptr, __u8 *buffer,
             unsigned long maxsize)
{
  unsigned long filesize = FAT2CPU32(dentptr->size), gotsize = 0;
  unsigned int bytesperclust = mydata->clust_size * SECTOR_SIZE;
  __u32 curclust = START(dentptr);
  __u32 endclust, newclust;
  unsigned long actsize;

#if 0
  FAT_DPRINT("Filesize: %ld bytes\n", filesize);
#endif
  DEBUG_L1_FS_FAT_FAT_OUTMSG("Filesize: ");
  DEBUG_L1_FS_FAT_FAT_OUTVLE(filesize, 0);
  DEBUG_L1_FS_FAT_FAT_OUTMSG("bytes\n");

  if (maxsize > 0 && filesize > maxsize) filesize = maxsize;

#if 0
  FAT_DPRINT("Reading: %ld bytes\n", filesize);
#endif
  DEBUG_L1_FS_FAT_FAT_OUTMSG("Reading: ");
  DEBUG_L1_FS_FAT_FAT_OUTVLE(filesize, 0);
  DEBUG_L1_FS_FAT_FAT_OUTMSG(" bytes\n");

  actsize=bytesperclust;
  endclust=curclust;
  do {
    /* search for consecutive clusters */
    while(actsize < filesize) {
      newclust = get_fatent(mydata, endclust);
      if((newclust -1)!=endclust)
        goto getit;
      if (CHECK_CLUST(newclust, mydata->fatsize)) {
#if 0
        FAT_DPRINT("curclust: 0x%x\n", newclust);
        FAT_DPRINT("Invalid FAT entry\n");
#endif
        DEBUG_L1_FS_FAT_FAT_OUTMSG("curclust: 0x");
        DEBUG_L1_FS_FAT_FAT_OUTVLE(newclust, 0);
        DEBUG_L1_FS_FAT_FAT_OUTMSG("\n");
        DEBUG_L1_FS_FAT_FAT_OUTMSG("Invalid FAT entry\n");
        return gotsize;
      }
      endclust=newclust;
      actsize+= bytesperclust;
    }
    /* actsize >= file size */
    actsize -= bytesperclust;
    /* get remaining clusters */
    if (get_cluster(mydata, curclust, buffer, (int)actsize) != 0) {
#if 0
      FAT_ERROR("Error reading cluster\n");
#endif
      DEBUG_L1_FS_FAT_FAT_OUTMSG("Error reading cluster\n");
      return -1;
    }
    /* get remaining bytes */
    gotsize += (int)actsize;
    filesize -= actsize;
    buffer += actsize;
    actsize= filesize;
    if (get_cluster(mydata, endclust, buffer, (int)actsize) != 0) {
#if 0
      FAT_ERROR("Error reading cluster\n");
#endif
      DEBUG_L1_FS_FAT_FAT_OUTMSG("Error reading cluster\n");
      return -1;
    }
    gotsize+=actsize;
    return gotsize;
  getit:
    if (get_cluster(mydata, curclust, buffer, (int)actsize) != 0) {
#if 0
      FAT_ERROR("Error reading cluster\n");
#endif
      DEBUG_L1_FS_FAT_FAT_OUTMSG("Error reading cluster\n");
      return -1;
    }
    gotsize += (int)actsize;
    filesize -= actsize;
    buffer += actsize;
    curclust = get_fatent(mydata, endclust);
    if (CHECK_CLUST(curclust, mydata->fatsize)) {
#if 0
      FAT_DPRINT("curclust: 0x%x\n", curclust);
      FAT_ERROR("Invalid FAT entry\n");
#endif
      DEBUG_L1_FS_FAT_FAT_OUTMSG("curclust: 0x");
      DEBUG_L1_FS_FAT_FAT_OUTVLE(curclust, 0);
      DEBUG_L1_FS_FAT_FAT_OUTMSG("\n");
      DEBUG_L1_FS_FAT_FAT_OUTMSG("Invalid FAT entry\n");
      return gotsize;
    }
    actsize=bytesperclust;
    endclust=curclust;
  } while (1);
}


#ifdef CONFIG_SUPPORT_VFAT
/*
 * Extract the file name information from 'slotptr' into 'l_name',
 * starting at l_name[*idx].
 * Return 1 if terminator (zero byte) is found, 0 otherwise.
 */
static int
slot2str(dir_slot *slotptr, char *l_name, int *idx)
{
  int j;

  for (j = 0; j <= 8; j += 2) {
    l_name[*idx] = slotptr->name0_4[j];
    if (l_name[*idx] == 0x00) return 1;
    (*idx)++;
  }
  for (j = 0; j <= 10; j += 2) {
    l_name[*idx] = slotptr->name5_10[j];
    if (l_name[*idx] == 0x00) return 1;
    (*idx)++;
  }
  for (j = 0; j <= 2; j += 2) {
    l_name[*idx] = slotptr->name11_12[j];
    if (l_name[*idx] == 0x00) return 1;
    (*idx)++;
  }

  return 0;
}


/*
 * Extract the full long filename starting at 'retdent' (which is really
 * a slot) into 'l_name'. If successful also copy the real directory entry
 * into 'retdent'
 * Return 0 on success, -1 otherwise.
 */
__attribute__ ((__aligned__(__alignof__(dir_entry))))
__u8 get_vfatname_block[MAX_CLUSTSIZE];
static int
get_vfatname(fsdata *mydata, int curclust, __u8 *cluster,
             dir_entry *retdent, char *l_name)
{
  dir_entry *realdent;
  dir_slot  *slotptr = (dir_slot*) retdent;
  __u8    *nextclust = cluster + mydata->clust_size * SECTOR_SIZE;
  __u8     counter = (slotptr->id & ~LAST_LONG_ENTRY_MASK) & 0xff;
  int idx = 0;

  while ((__u8*)slotptr < nextclust) {
    if (counter == 0) break;
    if (((slotptr->id & ~LAST_LONG_ENTRY_MASK) & 0xff) != counter)
      return -1;
    slotptr++;
    counter--;
  }

  if ((__u8*)slotptr >= nextclust) {
    dir_slot *slotptr2;

    slotptr--;
    curclust = get_fatent(mydata, curclust);
    if (CHECK_CLUST(curclust, mydata->fatsize)) {
#if 0
      FAT_DPRINT("curclust: 0x%x\n", curclust);
      FAT_ERROR("Invalid FAT entry\n");
#endif
      DEBUG_L1_FS_FAT_FAT_OUTMSG("curclust: 0x");
      DEBUG_L1_FS_FAT_FAT_OUTVLE(curclust, 0);
      DEBUG_L1_FS_FAT_FAT_OUTMSG("\n");
      DEBUG_L1_FS_FAT_FAT_OUTMSG("Invalid FAT entry\n");
      return -1;
    }
    if (get_cluster(mydata, curclust, get_vfatname_block,
                    mydata->clust_size * SECTOR_SIZE) != 0) {
#if 0
      FAT_DPRINT("Error: reading directory block\n");
#endif
      DEBUG_L1_FS_FAT_FAT_OUTMSG("Error: reading directory block\n");
      return -1;
    }
    slotptr2 = (dir_slot*) get_vfatname_block;
    while (slotptr2->id > 0x01) {
      slotptr2++;
    }
    /* Save the real directory entry */
    realdent = (dir_entry*)slotptr2 + 1;
    while ((__u8*)slotptr2 >= get_vfatname_block) {
      slot2str(slotptr2, l_name, &idx);
      slotptr2--;
    }
  } else {
    /* Save the real directory entry */
    realdent = (dir_entry*)slotptr;
  }

  do {
    slotptr--;
    if (slot2str(slotptr, l_name, &idx)) break;
  } while (!(slotptr->id & LAST_LONG_ENTRY_MASK));

  l_name[idx] = '\0';
  if (*l_name == DELETED_FLAG) *l_name = '\0';
  else if (*l_name == aRING) *l_name = DELETED_FLAG;
  downcase(l_name);

  /* Return the real directory entry */
  memcpy(retdent, realdent, sizeof(dir_entry));

  return 0;
}


/* Calculate short name checksum */
static __u8
mkcksum(const char *str)
{
  int i;
  __u8 ret = 0;

  for (i = 0; i < 11; i++) {
    ret = (((ret&1)<<7)|((ret&0xfe)>>1)) + str[i];
  }

  return ret;
}
#endif


/*
 * Get the directory entry associated with 'filename' from the directory
 * starting at 'startsect'
 */
__attribute__ ((__aligned__(__alignof__(dir_entry))))
__u8 get_dentfromdir_block[MAX_CLUSTSIZE];
static dir_entry *get_dentfromdir (fsdata * mydata, int startsect,
                                   char *filename, dir_entry * retdent,
                                   int dols)
{
  __u16 prevcksum = 0xffff;
  __u32 curclust = START (retdent);
  int files = 0, dirs = 0;

#if 0
  FAT_DPRINT ("get_dentfromdir: %s\n", filename);
#endif
  DEBUG_L1_FS_FAT_FAT_OUTMSG("get_dentfromdir: ");
  DEBUG_L1_FS_FAT_FAT_OUTMSG(filename);
  DEBUG_L1_FS_FAT_FAT_OUTMSG("\n");
  while (1) {
    dir_entry *dentptr;
    int i;

    if (get_cluster (mydata, curclust, get_dentfromdir_block,
                     mydata->clust_size * SECTOR_SIZE) != 0) {
#if 0
      FAT_DPRINT ("Error: reading directory block\n");
#endif
      DEBUG_L1_FS_FAT_FAT_OUTMSG("Error: reading directory block\n");
      return NULL;
    }
    dentptr = (dir_entry *) get_dentfromdir_block;
    for (i = 0; i < DIRENTSPERCLUST; i++) {
      char s_name[14], l_name[256];

      l_name[0] = '\0';
      if (dentptr->name[0] == DELETED_FLAG) {
        dentptr++;
        continue;
      }
      if ((dentptr->attr & ATTR_VOLUME)) {
#ifdef CONFIG_SUPPORT_VFAT
        if ((dentptr->attr & ATTR_VFAT) &&
            (dentptr->name[0] & LAST_LONG_ENTRY_MASK)) {
          prevcksum = ((dir_slot *) dentptr)
            ->alias_checksum;
          get_vfatname (mydata, curclust, get_dentfromdir_block,
                        dentptr, l_name);
          if (dols) {
            int isdir = (dentptr->attr & ATTR_DIR);
            char dirc;
            int doit = 0;

            if (isdir) {
              dirs++;
              dirc = '/';
              doit = 1;
            } else {
              dirc = ' ';
              if (l_name[0] != 0) {
                files++;
                doit = 1;
              }
            }
            if (doit) {
              if (dirc == ' ') {
                puts(" ");
                putxval((unsigned long)FAT2CPU32(dentptr->size), 8);
                puts("  ");
                puts(l_name);
                putc(dirc);
                puts("\n");
              } else {
                puts("            ");
                puts(l_name);
                putc(dirc);
                puts("\n");
              }
            }
            dentptr++;
            continue;
          }
#if 0
          FAT_DPRINT ("vfatname: |%s|\n", l_name);
#endif
          DEBUG_L1_FS_FAT_FAT_OUTMSG("vfatname: |");
          DEBUG_L1_FS_FAT_FAT_OUTMSG(l_name);
          DEBUG_L1_FS_FAT_FAT_OUTMSG("\n");
        } else
#endif
          {
            /* Volume label or VFAT entry */
            dentptr++;
            continue;
          }
      }
      if (dentptr->name[0] == 0) {
        if (dols) {
          puts("\n");
          putxval(files, 0);
          puts(" file(s), ");
          putxval(dirs, 0);
          puts(" dir(s)\n\n");
        }
#if 0
        FAT_DPRINT ("Dentname == NULL - %d\n", i);
#endif
        DEBUG_L1_FS_FAT_FAT_OUTMSG("Dentname == NULL - ");
        DEBUG_L1_FS_FAT_FAT_OUTVLE(i, 0);
        DEBUG_L1_FS_FAT_FAT_OUTMSG("\n");
        return NULL;
      }
#ifdef CONFIG_SUPPORT_VFAT
      if (dols && mkcksum (dentptr->name) == prevcksum) {
        dentptr++;
        continue;
      }
#endif
      get_name (dentptr, s_name);
      if (dols) {
        int isdir = (dentptr->attr & ATTR_DIR);
        char dirc;
        int doit = 0;

        if (isdir) {
          dirs++;
          dirc = '/';
          doit = 1;
        } else {
          dirc = ' ';
          if (s_name[0] != 0) {
            files++;
            doit = 1;
          }
        }
        if (doit) {
          if (dirc == ' ') {
            puts(" ");
            putxval((long)FAT2CPU32(dentptr->size), 8);
            puts("   ");
            puts(s_name);
            putc(dirc);
            puts("\n");
          } else {
            puts("            ");
            puts(s_name);
            putc(dirc);
            puts("\n");
          }
        }
        dentptr++;
        continue;
      }
      if (strcmp (filename, s_name) && strcmp (filename, l_name)) {
#if 0
        FAT_DPRINT ("Mismatch: |%s|%s|\n", s_name, l_name);
#endif
        DEBUG_L1_FS_FAT_FAT_OUTMSG("Mismatch: |");
        DEBUG_L1_FS_FAT_FAT_OUTMSG(s_name);
        DEBUG_L1_FS_FAT_FAT_OUTMSG(l_name);
        DEBUG_L1_FS_FAT_FAT_OUTMSG("\n");
        dentptr++;
        continue;
      }
      memcpy (retdent, dentptr, sizeof (dir_entry));
#if 0
      FAT_DPRINT ("DentName: %s", s_name);
      FAT_DPRINT (", start: 0x%x", START (dentptr));
      FAT_DPRINT (", size:  0x%x %s\n",
                  FAT2CPU32 (dentptr->size),
                  (dentptr->attr & ATTR_DIR) ? "(DIR)" : "");
#endif
      DEBUG_L1_FS_FAT_FAT_OUTMSG("DentName: ");
      DEBUG_L1_FS_FAT_FAT_OUTMSG(s_name);
      DEBUG_L1_FS_FAT_FAT_OUTMSG(", start: 0x");
      DEBUG_L1_FS_FAT_FAT_OUTVLE(START (dentptr), 0);
      DEBUG_L1_FS_FAT_FAT_OUTMSG(", size:  0x");
      DEBUG_L1_FS_FAT_FAT_OUTVLE(FAT2CPU32 (dentptr->size), 0);
      DEBUG_L1_FS_FAT_FAT_OUTMSG(" ");
      DEBUG_L1_FS_FAT_FAT_OUTMSG(((dentptr->attr & ATTR_DIR) ? "(DIR)" : ""));
      DEBUG_L1_FS_FAT_FAT_OUTMSG("\n");

      return retdent;
    }
    curclust = get_fatent (mydata, curclust);
    if (CHECK_CLUST(curclust, mydata->fatsize)) {
#if 0
      FAT_DPRINT ("curclust: 0x%x\n", curclust);
      FAT_ERROR ("Invalid FAT entry\n");
#endif
      DEBUG_L1_FS_FAT_FAT_OUTMSG("curclust: 0x");
      DEBUG_L1_FS_FAT_FAT_OUTVLE(curclust, 0);
      DEBUG_L1_FS_FAT_FAT_OUTMSG("Invalid FAT entry\n");
      return NULL;
    }
  }

  return NULL;
}


/*
 * Read boot sector and volume info from a FAT filesystem
 */
static int
read_bootsectandvi(boot_sector *bs, volume_info *volinfo, int *fatsize)
{
  __u8 block[FS_BLOCK_SIZE];
  volume_info *vistart;

  if (disk_read(0, 1, block) < 0) {
#if 0
    FAT_DPRINT("Error: reading block\n");
#endif
    DEBUG_L1_FS_FAT_FAT_OUTMSG("Error: reading block\n");
    return -1;
  }

  memcpy(bs, block, sizeof(boot_sector));
  bs->reserved  = FAT2CPU16(bs->reserved);
  bs->fat_length  = FAT2CPU16(bs->fat_length);
  bs->secs_track  = FAT2CPU16(bs->secs_track);
  bs->heads = FAT2CPU16(bs->heads);
#if 0 /* UNUSED */
  bs->hidden  = FAT2CPU32(bs->hidden);
#endif
  bs->total_sect  = FAT2CPU32(bs->total_sect);

  /* FAT32 entries */
  if (bs->fat_length == 0) {
    /* Assume FAT32 */
    bs->fat32_length = FAT2CPU32(bs->fat32_length);
    bs->flags  = FAT2CPU16(bs->flags);
    bs->root_cluster = FAT2CPU32(bs->root_cluster);
    bs->info_sector  = FAT2CPU16(bs->info_sector);
    bs->backup_boot  = FAT2CPU16(bs->backup_boot);
    vistart = (volume_info*) (block + sizeof(boot_sector));
    *fatsize = 32;
  } else {
    vistart = (volume_info*) &(bs->fat32_length);
    *fatsize = 0;
  }
  memcpy(volinfo, vistart, sizeof(volume_info));

  if (*fatsize == 32) {
    if (strncmp(FAT32_SIGN, vistart->fs_type, SIGNLEN) == 0) {
      return 0;
    }
  } else {
    if (strncmp(FAT12_SIGN, vistart->fs_type, SIGNLEN) == 0) {
      *fatsize = 12;
      return 0;
    }
    if (strncmp(FAT16_SIGN, vistart->fs_type, SIGNLEN) == 0) {
      *fatsize = 16;
      return 0;
    }
  }

#if 0
  FAT_DPRINT("Error: broken fs_type sign\n");
#endif
  DEBUG_L1_FS_FAT_FAT_OUTMSG("Error: broken fs_type sign\n");
  return -1;
}

__attribute__ ((__aligned__(__alignof__(dir_entry))))
__u8 do_fat_read_block[MAX_CLUSTSIZE];
long
do_fat_read (const char *filename, void *buffer, unsigned long maxsize,
             int dols)
{
  char fnamecopy[2048];
  boot_sector bs;
  volume_info volinfo;
  fsdata datablock;
  fsdata *mydata = &datablock;
  dir_entry *dentptr;
  __u16 prevcksum = 0xffff;
  char *subname = "";
  int rootdir_size, cursect;
  int idx, isdir = 0;
  int files = 0, dirs = 0;
  long ret = 0;
  int firsttime;

  if (read_bootsectandvi (&bs, &volinfo, &mydata->fatsize)) {
#if 0
    FAT_DPRINT ("Error: reading boot sector\n");
#endif
    DEBUG_L1_FS_FAT_FAT_OUTMSG("Error: reading boot sector\n");
    return -1;
  }
  if (mydata->fatsize == 32) {
    mydata->fatlength = bs.fat32_length;
  } else {
    mydata->fatlength = bs.fat_length;
  }
  mydata->fat_sect = bs.reserved;
  cursect = mydata->rootdir_sect
    = mydata->fat_sect + mydata->fatlength * bs.fats;
  mydata->clust_size = bs.cluster_size;
  if (mydata->fatsize == 32) {
    rootdir_size = mydata->clust_size;
    mydata->data_begin = mydata->rootdir_sect   /* + rootdir_size */
      - (mydata->clust_size * 2);
  } else {
    rootdir_size = ((bs.dir_entries[1] * (int) 256 + bs.dir_entries[0])
                    * sizeof (dir_entry)) / SECTOR_SIZE;
    mydata->data_begin = mydata->rootdir_sect + rootdir_size
      - (mydata->clust_size * 2);
  }
  mydata->fatbufnum = -1;
#if 0
  FAT_DPRINT ("FAT%d, fatlength: %d\n", mydata->fatsize,
              mydata->fatlength);
#endif
  DEBUG_L1_FS_FAT_FAT_OUTMSG("FAT");
  DEBUG_L1_FS_FAT_FAT_OUTVLE(mydata->fatsize, 0);
  DEBUG_L1_FS_FAT_FAT_OUTMSG(", fatlength: ");
  DEBUG_L1_FS_FAT_FAT_OUTVLE(mydata->fatlength, 0);
  DEBUG_L1_FS_FAT_FAT_OUTMSG("\n");
#if 0
  FAT_DPRINT ("Rootdir begins at sector: %d, offset: %x, size: %d\n"
              "Data begins at: %d\n",
              mydata->rootdir_sect, mydata->rootdir_sect * SECTOR_SIZE,
              rootdir_size, mydata->data_begin);
#endif
  DEBUG_L1_FS_FAT_FAT_OUTMSG("Rootdir begins at sector: ");
  DEBUG_L1_FS_FAT_FAT_OUTVLE(mydata->rootdir_sect, 0);
  DEBUG_L1_FS_FAT_FAT_OUTMSG(", offset: ");
  DEBUG_L1_FS_FAT_FAT_OUTVLE((mydata->rootdir_sect * SECTOR_SIZE), 0);
  DEBUG_L1_FS_FAT_FAT_OUTMSG(", size: ");
  DEBUG_L1_FS_FAT_FAT_OUTVLE(rootdir_size, 0);
  DEBUG_L1_FS_FAT_FAT_OUTMSG("\n");
  DEBUG_L1_FS_FAT_FAT_OUTMSG("Data begins at: ");
  DEBUG_L1_FS_FAT_FAT_OUTVLE(mydata->data_begin, 0);
  DEBUG_L1_FS_FAT_FAT_OUTMSG("\n");
#if 0
  FAT_DPRINT ("Cluster size: %d\n", mydata->clust_size);
#endif
  DEBUG_L1_FS_FAT_FAT_OUTMSG("Cluster size: ");
  DEBUG_L1_FS_FAT_FAT_OUTVLE(mydata->clust_size, 0);
  DEBUG_L1_FS_FAT_FAT_OUTMSG("\n");

  /* "cwd" is always the root... */
  while (ISDIRDELIM (*filename))
    filename++;
  /* Make a copy of the filename and convert it to lowercase */
  strcpy (fnamecopy, filename);
  downcase (fnamecopy);
  if (*fnamecopy == '\0') {
    if (!dols)
      return -1;
    dols = LS_ROOT;
  } else if ((idx = dirdelim (fnamecopy)) >= 0) {
    isdir = 1;
    fnamecopy[idx] = '\0';
    subname = fnamecopy + idx + 1;
    /* Handle multiple delimiters */
    while (ISDIRDELIM (*subname))
      subname++;
  } else if (dols) {
    isdir = 1;
  }

  while (1) {
    int i;

    if (disk_read (cursect, mydata->clust_size, do_fat_read_block) < 0) {
#if 0
      FAT_DPRINT ("Error: reading rootdir block\n");
#endif
      DEBUG_L1_FS_FAT_FAT_OUTMSG("Error: reading rootdir block\n");
      return -1;
    }
    dentptr = (dir_entry *) do_fat_read_block;
    for (i = 0; i < DIRENTSPERBLOCK; i++) {
      char s_name[14], l_name[256];

      l_name[0] = '\0';
      if ((dentptr->attr & ATTR_VOLUME)) {
#ifdef CONFIG_SUPPORT_VFAT
        if ((dentptr->attr & ATTR_VFAT) &&
            (dentptr->name[0] & LAST_LONG_ENTRY_MASK)) {
          prevcksum = ((dir_slot *) dentptr)->alias_checksum;
          get_vfatname (mydata, 0, do_fat_read_block, dentptr, l_name);
          if (dols == LS_ROOT) {
            int isdir = (dentptr->attr & ATTR_DIR);
            char dirc;
            int doit = 0;

            if (isdir) {
              dirs++;
              dirc = '/';
              doit = 1;
            } else {
              dirc = ' ';
              if (l_name[0] != 0) {
                files++;
                doit = 1;
              }
            }
            if (doit) {
              if (dirc == ' ') {
                puts(" ");
                putxval((long)FAT2CPU32(dentptr->size), 8);
                puts("   ");
                puts(l_name);
                putc(dirc);
                puts("\n");
              } else {
                puts("            ");
                puts(l_name);
                putc(dirc);
                puts("\n");
              }
            }
            dentptr++;
            continue;
          }
#if 0
          FAT_DPRINT ("Rootvfatname: |%s|\n", l_name);
#endif
          DEBUG_L1_FS_FAT_FAT_OUTMSG("Rootvfatname: |");
          DEBUG_L1_FS_FAT_FAT_OUTMSG(l_name);
          DEBUG_L1_FS_FAT_FAT_OUTMSG("|\n");
        } else
#endif
          {
            /* Volume label or VFAT entry */
            dentptr++;
            continue;
          }
      } else if (dentptr->name[0] == 0) {
#if 0
        FAT_DPRINT ("RootDentname == NULL - %d\n", i);
#endif
        DEBUG_L1_FS_FAT_FAT_OUTMSG("RootDentname == NULL - ");
        DEBUG_L1_FS_FAT_FAT_OUTVLE(i, 0);
        DEBUG_L1_FS_FAT_FAT_OUTMSG("\n");
        if (dols == LS_ROOT) {
          puts("\n");
          putxval(files, 0);
          puts(" file(s), ");
          putxval(dirs, 0);
          puts(" dir(s)\n\n");
          return 0;
        }
        return -1;
      }
#ifdef CONFIG_SUPPORT_VFAT
      else if (dols == LS_ROOT
               && mkcksum (dentptr->name) == prevcksum) {
        dentptr++;
        continue;
      }
#endif
      get_name (dentptr, s_name);
      if (dols == LS_ROOT) {
        int isdir = (dentptr->attr & ATTR_DIR);
        char dirc;
        int doit = 0;

        if (isdir) {
          dirc = '/';
          if (s_name[0] != 0) {
            dirs++;
            doit = 1;
          }
        } else {
          dirc = ' ';
          if (s_name[0] != 0) {
            files++;
            doit = 1;
          }
        }
        if (doit) {
          if (dirc == ' ') {
            puts(" ");
            putxval((long) FAT2CPU32 (dentptr->size), 8);
            puts("   ");
            puts(s_name);
            putc(dirc);
            puts("\n");
          } else {
            puts("            ");
            puts(s_name);
            putc(dirc);
            puts("\n");
          }
        }
        dentptr++;
        continue;
      }
      if (strcmp (fnamecopy, s_name) && strcmp (fnamecopy, l_name)) {
#if 0
        FAT_DPRINT ("RootMismatch: |%s|%s|\n", s_name, l_name);
#endif
        DEBUG_L1_FS_FAT_FAT_OUTMSG("RootMismatch: |");
        DEBUG_L1_FS_FAT_FAT_OUTMSG(s_name);
        DEBUG_L1_FS_FAT_FAT_OUTMSG(l_name);
        DEBUG_L1_FS_FAT_FAT_OUTMSG("\n");
        dentptr++;
        continue;
      }
      if (isdir && !(dentptr->attr & ATTR_DIR))
        return -1;
#if 0
      FAT_DPRINT ("RootName: %s", s_name);
      FAT_DPRINT (", start: 0x%x", START (dentptr));
      FAT_DPRINT (", size:  0x%x %s\n",
                  FAT2CPU32 (dentptr->size), isdir ? "(DIR)" : "");
#endif
      DEBUG_L1_FS_FAT_FAT_OUTMSG("RootName: ");
      DEBUG_L1_FS_FAT_FAT_OUTMSG(s_name);
      DEBUG_L1_FS_FAT_FAT_OUTMSG(", start: 0x");
      DEBUG_L1_FS_FAT_FAT_OUTVLE(START (dentptr), 0);
      DEBUG_L1_FS_FAT_FAT_OUTMSG(", size:  0x");
      DEBUG_L1_FS_FAT_FAT_OUTVLE(FAT2CPU32 (dentptr->size), 0);
      DEBUG_L1_FS_FAT_FAT_OUTMSG(" ");
      DEBUG_L1_FS_FAT_FAT_OUTMSG((isdir ? "(DIR)" : ""));
      DEBUG_L1_FS_FAT_FAT_OUTMSG("\n");

      goto rootdir_done;  /* We got a match */
    }
    cursect++;
  }
 rootdir_done:

  firsttime = 1;
  while (isdir) {
    int startsect = mydata->data_begin
      + START (dentptr) * mydata->clust_size;
    dir_entry dent;
    char *nextname = NULL;

    dent = *dentptr;
    dentptr = &dent;

    idx = dirdelim (subname);
    if (idx >= 0) {
      subname[idx] = '\0';
      nextname = subname + idx + 1;
      /* Handle multiple delimiters */
      while (ISDIRDELIM (*nextname))
        nextname++;
      if (dols && *nextname == '\0')
        firsttime = 0;
    } else {
      if (dols && firsttime) {
        firsttime = 0;
      } else {
        isdir = 0;
      }
    }

    if (get_dentfromdir (mydata, startsect, subname, dentptr,
                         isdir ? 0 : dols) == NULL) {
      if (dols && !isdir)
        return 0;
      return -1;
    }

    if (idx >= 0) {
      if (!(dentptr->attr & ATTR_DIR))
        return -1;
      subname = nextname;
    }
  }
  ret = get_contents (mydata, dentptr, buffer, maxsize);
#if 0
  FAT_DPRINT ("Size: %d, got: %ld\n", FAT2CPU32 (dentptr->size), ret);
#endif
  DEBUG_L1_FS_FAT_FAT_OUTMSG("Size: ");
  DEBUG_L1_FS_FAT_FAT_OUTVLE(FAT2CPU32 (dentptr->size), 0);
  DEBUG_L1_FS_FAT_FAT_OUTMSG(", got: ");
  DEBUG_L1_FS_FAT_FAT_OUTVLE(ret, 0);
  DEBUG_L1_FS_FAT_FAT_OUTMSG("\n");

  return ret;
}


int
file_fat_detectfs(void)
{
  boot_sector bs;
  volume_info volinfo;
  int   fatsize;
  char  vol_label[12];

  if(cur_dev==NULL) {
    puts("No current device\n");
    return 1;
  }
#if defined(CONFIG_CMD_IDE) ||                  \
  defined(CONFIG_CMD_MG_DISK) ||                \
  defined(CONFIG_CMD_SATA) ||                   \
  defined(CONFIG_CMD_SCSI) ||                   \
  defined(CONFIG_CMD_USB) ||                    \
  defined(CONFIG_MMC)
  puts("Interface:  ");
  switch(cur_dev->if_type) {
  case IF_TYPE_IDE :  puts("IDE"); break;
  case IF_TYPE_SATA : puts("SATA"); break;
  case IF_TYPE_SCSI : puts("SCSI"); break;
  case IF_TYPE_ATAPI :  puts("ATAPI"); break;
  case IF_TYPE_USB :  puts("USB"); break;
  case IF_TYPE_DOC :  puts("DOC"); break;
  case IF_TYPE_MMC :  puts("MMC"); break;
  default :   puts("Unknown");
  }
  puts("\n  Device ");
  putxval(cur_dev->dev, 0);
  puts(" %d: ");
  dev_print(cur_dev);
#endif
  if(read_bootsectandvi(&bs, &volinfo, &fatsize)) {
    puts("\nNo valid FAT fs found\n");
    return 1;
  }
  memcpy (vol_label, volinfo.volume_label, 11);
  vol_label[11] = '\0';
  volinfo.fs_type[5]='\0';
  puts("Partition ");
  putxval(cur_part, 0);
  puts(": Filesystem: ");
  puts(volinfo.fs_type);
  puts(" \"");
  puts(vol_label);
  puts("\"\n");
  return 0;
}


int
file_fat_ls(const char *dir)
{
  return do_fat_read(dir, NULL, 0, LS_YES);
}


long
file_fat_read(const char *filename, void *buffer, unsigned long maxsize)
{
  puts("reading ");
  puts((char *)filename);
  puts("\n");
  return do_fat_read(filename, buffer, maxsize, LS_NO);
}
