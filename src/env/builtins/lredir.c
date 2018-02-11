/*
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/***********************************************
 * File: LREDIR.C
 *  Program for Linux DOSEMU disk redirector functions
 * Written: 10/29/93 by Tim Bird
 * Fixes: 1/7/95 by Tim Josling TEJ: check correct # args
 *      :                            display read/write status correctly
 * Changes: 3/19/95 by Kang-Jin Lee
 *  Removed unused variables
 *  Changes to eliminate warnings
 *  Display read/write status when redirecting drive
 *  Usage information is more verbose
 *
 * Changes: 11/27/97 Hans Lermen
 *  new detection stuff,
 *  rewrote to use plain C and intr(), instead of asm and fiddling with
 *  (maybe used) machine registers.
 *
 * Changes: 990703 Hans Lermen
 *  Checking for DosC and exiting with error, if not redirector capable.
 *  Printing DOS errors in clear text.
 *
 * Changes: 010211 Bart Oldeman
 *   Make it possible to "reredirect": LREDIR drive filepath
 *   can overwrite the direction for the drive.
 *
 * Changes: 20010402 Hans Lermen
 *   Ported to buildin_apps, compiled directly into dosemu
 *
 * lredir2 is written by Stas Sergeev
 *
 ***********************************************/


#include "config.h"
#include <stdio.h>    /* printf  */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "emu.h"
#include "memory.h"
#include "msetenv.h"
#include "doshelpers.h"
#include "utilities.h"
#include "builtins.h"
#include "lpt.h"
#include "disks.h"
#include "redirect.h"
#include "lredir.h"

#define printf	com_printf
#define	intr	com_intr
#define	strncmpi strncasecmp
#define FP_OFF(x) DOSEMU_LMHEAP_OFFS_OF(x)
#define FP_SEG(x) DOSEMU_LMHEAP_SEG

typedef unsigned char uint8;
typedef unsigned int uint16;

#define CARRY_FLAG    1 /* carry bit in flags register */
#define CC_SUCCESS    0

#define DOS_GET_DEFAULT_DRIVE  0x1900
#define DOS_GET_CWD            0x4700
#define DOS_GET_REDIRECTION    0x5F02
#define DOS_REDIRECT_DEVICE    0x5F03
#define DOS_CANCEL_REDIRECTION 0x5F04

#define MAX_RESOURCE_STRING_LENGTH  36  /* 16 + 16 + 3 for slashes + 1 for NULL */
#define MAX_RESOURCE_PATH_LENGTH   128  /* added to support Linux paths */
#define MAX_DEVICE_STRING_LENGTH     5  /* enough for printer strings */

#define REDIR_PRINTER_TYPE    3
#define REDIR_DISK_TYPE       4

#define READ_ONLY_DRIVE_ATTRIBUTE 1  /* same as NetWare Lite */

#define KEYWORD_DEL   "DELETE"
#define KEYWORD_DEL_COMPARE_LENGTH  3

#define KEYWORD_HELP   "HELP"
#define KEYWORD_HELP_COMPARE_LENGTH  4

#define DEFAULT_REDIR_PARAM   0

#include "doserror.h"


static int isInitialisedMFS(void)
{
    struct vm86_regs preg;

    preg.ebx = DOS_SUBHELPER_MFS_REDIR_STATE;
    if (mfs_helper(&preg) == TRUE) {
       return ((preg.eax & 0xffff) == 1);
    }
    return 0;
}

/********************************************
 * RedirectDevice - redirect a device to a remote resource
 * ON ENTRY:
 *  deviceStr has a string with the device name:
 *    either disk or printer (ex. 'D:' or 'LPT1')
 *  resourceStr has a string with the server and name of resource
 *    (ex. 'TIM\TOOLS')
 *  deviceType indicates the type of device being redirected
 *    3 = printer, 4 = disk
 *  deviceParameter is a value to be saved with this redirection
 *  which will be returned on GetRedirectionList
 * ON EXIT:
 *  returns CC_SUCCESS if the operation was successful,
 *  otherwise returns the DOS error code
 * NOTES:
 *  deviceParameter is used in DOSEMU to return the drive attribute
 *  It is not actually saved and returned as specified by the redirector
 *  specification.  This type of usage is common among commercial redirectors.
 ********************************************/
static uint16 RedirectDevice(char *deviceStr, char *slashedResourceStr, uint8 deviceType,
                      uint16 deviceParameter)
{
    struct REGPACK preg = REGPACK_INIT;
    char *dStr, *sStr;

    /* should verify strings before sending them down ??? */
    dStr = com_strdup(deviceStr);
    preg.r_ds = FP_SEG(dStr);
    preg.r_si = FP_OFF(dStr);
    sStr = com_strdup(slashedResourceStr);
    preg.r_es = FP_SEG(sStr);
    preg.r_di = FP_OFF(sStr);
    preg.r_cx = deviceParameter;
    preg.r_bx = deviceType;
    preg.r_ax = DOS_REDIRECT_DEVICE;
    intr(0x21, &preg);
    com_strfree(dStr);
    com_strfree(sStr);

    if (preg.r_flags & CARRY_FLAG) {
      return (preg.r_ax);
    }
    else {
      return (CC_SUCCESS);
    }
}

/********************************************
 * GetRedirection - get next entry from list of redirected devices
 * ON ENTRY:
 *  redirIndex has the index of the next device to return
 *    this should start at 0, and be incremented between calls
 *    to retrieve all elements of the redirection list
 * ON EXIT:
 *  returns CC_SUCCESS if the operation was successful, and
 *  deviceStr has a string with the device name:
 *    either disk or printer (ex. 'D:' or 'LPT1')
 *  resourceStr has a string with the server and name of resource
 *    (ex. 'TIM\TOOLS')
 *  deviceType indicates the type of device which was redirected
 *    3 = printer, 4 = disk
 *  deviceParameter has my rights to this resource
 * NOTES:
 *
 ********************************************/
static uint16 GetRedirection(uint16 redirIndex, char *deviceStr, char *slashedResourceStr,
                      uint8 * deviceType, uint16 * deviceParameter)
{
    uint16 ccode;
    uint8 deviceTypeTemp;
    struct REGPACK preg = REGPACK_INIT;
    char *dStr, *sStr;

    dStr = lowmem_alloc(16);
    preg.r_ds = FP_SEG(dStr);
    preg.r_si = FP_OFF(dStr);
    sStr = lowmem_alloc(128);
    preg.r_es = FP_SEG(sStr);
    preg.r_di = FP_OFF(sStr);
    preg.r_bx = redirIndex;
    preg.r_ax = DOS_GET_REDIRECTION;
    intr(0x21, &preg);
    strcpy(deviceStr,dStr);
    lowmem_free(dStr, 16);
    strcpy(slashedResourceStr,sStr);
    lowmem_free(sStr, 128);

    ccode = preg.r_ax;
    deviceTypeTemp = preg.r_bx & 0xff;       /* save device type before C ruins it */
    *deviceType = deviceTypeTemp;
    *deviceParameter = preg.r_cx;

    /* copy back unslashed portion of resource string */
    if (preg.r_flags & CARRY_FLAG) {
      return (ccode);
    }
    else {
      return (CC_SUCCESS);
    }
}

static int getCWD(char *rStr, int len)
{
    char *cwd;
    struct REGPACK preg = REGPACK_INIT;
    uint8_t drive;

    preg.r_ax = DOS_GET_DEFAULT_DRIVE;
    intr(0x21, &preg);
    drive = preg.r_ax & 0xff;

    cwd = lowmem_alloc(64);
    preg.r_ax = DOS_GET_CWD;
    preg.r_dx = 0;
    preg.r_ds = FP_SEG(cwd);
    preg.r_si = FP_OFF(cwd);
    intr(0x21, &preg);
    if (preg.r_flags & CARRY_FLAG) {
	lowmem_free(cwd, 64);
	return preg.r_ax ?: -1;
    }

    if (cwd[0]) {
        snprintf(rStr, len, "%c:\\%s", 'A' + drive, cwd);
    } else {
        snprintf(rStr, len, "%c:", 'A' + drive);
    }
    lowmem_free(cwd, 64);
    return 0;
}

/********************************************
 * CancelRedirection - delete a device mapped to a remote resource
 * ON ENTRY:
 *  deviceStr has a string with the device name:
 *    either disk or printer (ex. 'D:' or 'LPT1')
 * ON EXIT:
 *  returns CC_SUCCESS if the operation was successful,
 *  otherwise returns the DOS error code
 * NOTES:
 *
 ********************************************/
static uint16 CancelRedirection(char *deviceStr)
{
    struct REGPACK preg = REGPACK_INIT;
    char *dStr;

    dStr = com_strdup(deviceStr);
    preg.r_ds = FP_SEG(dStr);
    preg.r_si = FP_OFF(dStr);
    preg.r_ax = DOS_CANCEL_REDIRECTION;
    intr(0x21, &preg);
    com_strfree(dStr);

    if (preg.r_flags & CARRY_FLAG) {
      return (preg.r_ax);
    }
    else {
      return (CC_SUCCESS);
    }
}

static int get_unix_cwd(char *buf)
{
    char dcwd[MAX_RESOURCE_PATH_LENGTH];
    int err;

    err = getCWD(dcwd, sizeof dcwd);
    if (err)
        return -1;

    err = build_posix_path(buf, dcwd, 0);
    if (err < 0)
        return -1;

    return 0;
}

/*************************************
 * ShowMyRedirections
 *  show my current drive redirections
 * NOTES:
 *  I show the read-only attribute for each drive
 *    which is returned in deviceParam.
 *************************************/
static void
ShowMyRedirections(void)
{
    int driveCount;
    uint16 redirIndex, deviceParam, ccode;
    uint8 deviceType;
    char deviceStr[MAX_DEVICE_STRING_LENGTH];
    char ucwd[MAX_RESOURCE_PATH_LENGTH];
    char resourceStr[MAX_RESOURCE_PATH_LENGTH];
    int err;

    redirIndex = 0;
    driveCount = 0;

    while ((ccode = GetRedirection(redirIndex, deviceStr, resourceStr,
                           &deviceType, &deviceParam)) == CC_SUCCESS) {
      /* only print disk redirections here */
      if (deviceType == REDIR_DISK_TYPE) {
        if (driveCount == 0) {
          printf("Current Drive Redirections:\n");
        }
        driveCount++;
        printf("%-2s = %-20s ", deviceStr, resourceStr);

        /* read attribute is returned in the device parameter */
        if (deviceParam & 0x80) {
	  if ((deviceParam & 0x7f) > 1)
	    printf("CDROM:%d ", (deviceParam & 0x7f) - 1);
          if (((deviceParam & 0x7f) != 0) == READ_ONLY_DRIVE_ATTRIBUTE)
	    printf("attrib = READ ONLY\n");
	  else
	    printf("attrib = READ/WRITE\n");
        }
      }

      redirIndex++;
    }

    if (driveCount == 0) {
      printf("No drives are currently redirected to Linux.\n");
    }

    err = get_unix_cwd(ucwd);
    if (err)
        return;
    printf("cwd: %s\n", ucwd);
}

static void
DeleteDriveRedirection(char *deviceStr)
{
    uint16 ccode;

    /* convert device string to upper case */
    strupperDOS(deviceStr);
    ccode = CancelRedirection(deviceStr);
    if (ccode) {
      printf("Error %x (%s) canceling redirection on drive %s\n",
             ccode, decode_DOS_error(ccode), deviceStr);
      }
    else {
      printf("Redirection for drive %s was deleted.\n", deviceStr);
    }
}

static int FindRedirectionByDevice(char *deviceStr, char *presourceStr)
{
    uint16 redirIndex = 0, deviceParam, ccode;
    uint8 deviceType;
    char dStr[MAX_DEVICE_STRING_LENGTH];
    char dStrSrc[MAX_DEVICE_STRING_LENGTH];

    snprintf(dStrSrc, MAX_DEVICE_STRING_LENGTH, "%s", deviceStr);
    strupperDOS(dStrSrc);
    while ((ccode = GetRedirection(redirIndex, dStr, presourceStr,
                           &deviceType, &deviceParam)) == CC_SUCCESS) {
      if (strcmp(dStrSrc, dStr) == 0)
        break;
      redirIndex++;
    }

    return ccode;
}

static int FindFATRedirectionByDevice(char *deviceStr, char *presourceStr)
{
    struct DINFO *di;
    char *dir;
    char *p;
    fatfs_t *f;
    int ret;

    if (!(di = (struct DINFO *)lowmem_alloc(sizeof(struct DINFO))))
	return 0;
    pre_msdos();
    LWORD(eax) = 0x6900;
    LWORD(ebx) = toupperDOS(deviceStr[0]) - 'A' + 1;
    SREG(ds) = FP_SEG(di);
    LWORD(edx) = FP_OFF(di);
    call_msdos();
    if (REG(eflags) & CF) {
	post_msdos();
	lowmem_free((void *)di, sizeof(struct DINFO));
	printf("error retrieving serial, %#x\n", LWORD(eax));
	return -1;
    }
    post_msdos();
    f = get_fat_fs_by_serial(READ_DWORDP((unsigned char *)&di->serial));
    lowmem_free((void *)di, sizeof(struct DINFO));
    if (!f || !(dir = f->dir)) {
	printf("error identifying FAT volume\n");
	return -1;
    }

    ret = sprintf(presourceStr, LINUX_RESOURCE "%s", dir);
    assert(ret != -1);
    p = presourceStr;
    while (*p) {
	if (*p == '/')
	    *p = '\\';
	p++;
    }
    *p++ = '\\';
    *p++ = '\0';

    return CC_SUCCESS;
}

static int do_repl(char *argv, char *resourceStr)
{
    int is_cwd, is_drv, ret;
    char *argv2;
    char deviceStr2[MAX_DEVICE_STRING_LENGTH];
    uint16_t ccode;

    is_cwd = (strncmp(argv, ".\\", 2) == 0);
    is_drv = argv[1] == ':';
    /* lredir c: d: */
    if (is_cwd) {
        char tmp[MAX_RESOURCE_PATH_LENGTH];
        int err = getCWD(tmp, sizeof tmp);
        if (err) {
          printf("Error: unable to get CWD\n");
          return 1;
        }
        ret = asprintf(&argv2, "%s\\%s", tmp, argv + 2);
        assert(ret != -1);
    } else if (is_drv) {
        argv2 = strdup(argv);
    } else {
        printf("Error: \"%s\" is not a DOS path\n", argv);
        return 1;
    }

    strncpy(deviceStr2, argv2, 2);
    deviceStr2[2] = 0;
    ccode = FindRedirectionByDevice(deviceStr2, resourceStr);
    if (ccode != CC_SUCCESS)
        ccode = FindFATRedirectionByDevice(deviceStr2, resourceStr);
    if (ccode != CC_SUCCESS) {
        printf("Error: unable to find redirection for drive %s\n", deviceStr2);
        free(argv2);
        return 1;
    }
    if (strlen(argv2) > 3)
        strcat(resourceStr, argv2 + 3);
    free(argv2);
    return 0;
}

struct lredir_opts {
    int help;
    int cdrom;
    int ro;
    int nd;
    int force;
    int pwd;
    char *del;
};

static int lredir_parse_opts(int argc, char *argv[],
	const char *getopt_string, struct lredir_opts *opts)
{
    char c;

    memset(opts, 0, sizeof(*opts));
    optind = 0;		// glibc wants this to reser parser state
    while ((c = getopt(argc, argv, getopt_string)) != EOF) {
	switch (c) {
	case 'h':
	    opts->help = 1;
	    break;

	case 'd':
	    opts->del = optarg;
	    break;

	case 'f':
	    opts->force = 1;
	    break;

	case 'C':
	    opts->cdrom = (optarg ? atoi(optarg) : 1);
	    break;

	case 'r':
	    printf("-r option deprecated, ignored\n");
	    break;

	case 'n':
	    opts->nd = 1;
	    break;

	case 'R':
	    opts->ro = 1;
	    break;

	case 'w':
	    opts->pwd = 1;
	    break;

	default:
	    printf("Unknown option %c\n", optopt);
	    return 1;
	}
    }

    if (!opts->help && !opts->del && argc < optind + 2 - opts->nd) {
	printf("missing arguments\n");
	return 1;
    }
    return 0;
}

static int fill_dev_str(char *deviceStr, char *argv,
	const struct lredir_opts *opts)
{
    if (!opts->nd) {
	if (argv[1] != ':' || argv[2]) {
	    printf("invalid argument %s\n", argv);
	    return 1;
	}
	strcpy(deviceStr, argv);
    } else {
	int nextDrive;
	nextDrive = find_free_drive();
	if (nextDrive < 0) {
		printf("Cannot redirect (maybe no drives available).");
		return 1;
	}
	deviceStr[0] = nextDrive + 'A';
	deviceStr[1] = ':';
	deviceStr[2] = '\0';
    }
    return 0;
}

static int do_redirect(char *deviceStr, char *resourceStr,
	const struct lredir_opts *opts)
{
    uint16 ccode;
    int deviceParam;

    if (opts->ro)
	deviceParam = 1;
    else if (opts->cdrom)
	deviceParam = 1 + opts->cdrom;
    else
	deviceParam = 0;

    /* upper-case both strings */
    strupperDOS(deviceStr);
    strupperDOS(resourceStr);

    /* now actually redirect the drive */
    ccode = RedirectDevice(deviceStr, resourceStr, REDIR_DISK_TYPE,
                           deviceParam);

    /* duplicate redirection: try to reredirect */
    if (ccode == 0x55) {
      if (!opts->force) {
        printf("Error: drive %s already redirected.\n"
               "       Use -d to delete the redirection or -f to force.\n",
               deviceStr);
        return 1;
      } else {
        DeleteDriveRedirection(deviceStr);
        ccode = RedirectDevice(deviceStr, resourceStr, REDIR_DISK_TYPE,
                             deviceParam);
      }
    }

    if (ccode) {
      printf("Error %x (%s) while redirecting drive %s to %s\n",
             ccode, decode_DOS_error(ccode), deviceStr, resourceStr);
      return 1;
    }

    printf("%s = %s", deviceStr, resourceStr);
    if (deviceParam > 1)
      printf(" CDROM:%d", deviceParam - 1);
    printf(" attrib = ");
    if ((deviceParam != 0) == READ_ONLY_DRIVE_ATTRIBUTE)
      printf("READ ONLY\n");
    else
      printf("READ/WRITE\n");

    return 0;
}

int lredir2_main(int argc, char **argv)
{
    int ret;
    char deviceStr[MAX_DEVICE_STRING_LENGTH];
    char resourceStr[MAX_RESOURCE_PATH_LENGTH];
    struct lredir_opts opts;
    const char *getopt_string = "fhd:C::Rrnw";

    /* check the MFS redirector supports this DOS */
    if (!isInitialisedMFS()) {
      printf("Unsupported DOS version or EMUFS.SYS not loaded\n");
      return(2);
    }

    /* need to parse the command line */
    /* if no parameters, then just show current mappings */
    if (argc == 1) {
      ShowMyRedirections();
      return(0);
    }

    ret = lredir_parse_opts(argc, argv, getopt_string, &opts);
    if (ret)
	return ret;

    if (opts.help) {
	printf("Usage: LREDIR2 <options> [drive:] [DOS_path]\n");
	printf("Redirect a drive to the specified DOS path.\n\n");
	printf("LREDIR X: C:\\tmp\n");
	printf("  Redirect drive X: to C:\\tmp\n");
	printf("  If -f is specified, the redirection is forced even if already redirected.\n");
	printf("  If -R is specified, the drive will be read-only\n");
	printf("  If -C is specified, (read-only) CDROM n is used (n=1 by default)\n");
	printf("  If -n is specified, the next available drive will be used.\n");
	printf("LREDIR -d drive:\n");
	printf("  delete a drive redirection\n");
	printf("LREDIR -w\n");
	printf("  show linux path for DOS CWD\n");
	printf("LREDIR\n");
	printf("  show current drive redirections\n");
	printf("LREDIR -h\n");
	printf("  show this help screen\n");
	return 0;
    }

    if (opts.del) {
	DeleteDriveRedirection(opts.del);
	return 0;
    }

    if (opts.pwd) {
	char ucwd[MAX_RESOURCE_PATH_LENGTH];
	int err = get_unix_cwd(ucwd);
	if (err)
	    return err;
	printf("%s\n", ucwd);
	return 0;
    }

    ret = fill_dev_str(deviceStr, argv[optind], &opts);
    if (ret)
	return ret;

    ret = do_repl(argv[optind + 1 - opts.nd], resourceStr);
    if (ret)
	return ret;

    return do_redirect(deviceStr, resourceStr, &opts);
}

int lredir_main(int argc, char **argv)
{
    int ret;
    char deviceStr[MAX_DEVICE_STRING_LENGTH];
    char resourceStr[MAX_RESOURCE_PATH_LENGTH];
    struct lredir_opts opts;
    const char *getopt_string = "fhd:C::Rn";

    /* check the MFS redirector supports this DOS */
    if (!isInitialisedMFS()) {
      printf("Unsupported DOS version or EMUFS.SYS not loaded\n");
      return(2);
    }

    /* need to parse the command line */
    /* if no parameters, then just show current mappings */
    if (argc == 1) {
      ShowMyRedirections();
      return(0);
    }

    ret = lredir_parse_opts(argc, argv, getopt_string, &opts);
    if (ret)
	return ret;

    if (opts.help) {
	printf("Usage: LREDIR <options> [drive:] [" LINUX_RESOURCE "\\path]\n");
	printf("Redirect a drive to the Linux file system.\n\n");
	printf("LREDIR X: " LINUX_RESOURCE "\\tmp\n");
	printf("  Redirect drive X: to /tmp of Linux file system for read/write\n");
	printf("  If -f is specified, the redirection is forced even if already redirected.\n");
	printf("  If -R is specified, the drive will be read-only\n");
	printf("  If -C is specified, (read-only) CDROM n is used (n=1 by default)\n");
	printf("  If -n is specified, the next available drive will be used.\n");
	printf("LREDIR -d drive:\n");
	printf("  delete a drive redirection\n");
	printf("LREDIR\n");
	printf("  show current drive redirections\n");
	printf("LREDIR -h\n");
	printf("  show this help screen\n");
	return 0;
    }

    if (opts.del) {
	DeleteDriveRedirection(opts.del);
	return 0;
    }

    ret = fill_dev_str(deviceStr, argv[optind], &opts);
    if (ret)
	return ret;

    resourceStr[0] = '\0';
    if (argv[optind + 1 - opts.nd][0] == '/')
	strcpy(resourceStr, LINUX_RESOURCE);
    /* accept old unslashed notation */
    else if (strncasecmp(argv[optind + 1 - opts.nd], LINUX_RESOURCE + 2,
		strlen(LINUX_RESOURCE) - 2) == 0)
	strcpy(resourceStr, "\\\\");
    strcat(resourceStr, argv[optind + 1 - opts.nd]);

    return do_redirect(deviceStr, resourceStr, &opts);
}
