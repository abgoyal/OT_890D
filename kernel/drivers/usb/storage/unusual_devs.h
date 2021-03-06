




UNUSUAL_DEV(  0x03eb, 0x2002, 0x0100, 0x0100,
		"ATMEL",
		"SND1 Storage",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE),

/* modified by Tobias Lorenz <tobias.lorenz@gmx.net> */
UNUSUAL_DEV(  0x03ee, 0x6901, 0x0000, 0x0200,
		"Mitsumi",
		"USB FDD",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_SINGLE_LUN ),

/* Reported by Rodolfo Quesada <rquesada@roqz.net> */
UNUSUAL_DEV(  0x03ee, 0x6906, 0x0003, 0x0003,
		"VIA Technologies Inc.",
		"Mitsumi multi cardreader",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

UNUSUAL_DEV(  0x03f0, 0x0107, 0x0200, 0x0200,
		"HP",
		"CD-Writer+",
		US_SC_8070, US_PR_CB, NULL, 0),

#ifdef CONFIG_USB_STORAGE_USBAT
UNUSUAL_DEV(  0x03f0, 0x0207, 0x0001, 0x0001,
		"HP",
		"CD-Writer+ 8200e",
		US_SC_8070, US_PR_USBAT, init_usbat_cd, 0),

UNUSUAL_DEV(  0x03f0, 0x0307, 0x0001, 0x0001,
		"HP",
		"CD-Writer+ CD-4e",
		US_SC_8070, US_PR_USBAT, init_usbat_cd, 0),
#endif

/* Reported by Ben Efros <ben@pc-doctor.com> */
UNUSUAL_DEV(  0x03f0, 0x070c, 0x0000, 0x0000,
		"HP",
		"Personal Media Drive",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_SANE_SENSE ),

UNUSUAL_DEV(  0x03f0, 0x4002, 0x0001, 0x0001,
		"HP",
		"PhotoSmart R707",
		US_SC_DEVICE, US_PR_DEVICE, NULL, US_FL_FIX_CAPACITY),

UNUSUAL_DEV(  0x0409, 0x0040, 0x0000, 0x9999,
		"NEC",
		"NEC USB UF000x",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_SINGLE_LUN ),

/* Patch submitted by Mihnea-Costin Grigore <mihnea@zulu.ro> */
UNUSUAL_DEV(  0x040d, 0x6205, 0x0003, 0x0003,
		"VIA Technologies Inc.",
		"USB 2.0 Card Reader",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

UNUSUAL_DEV(  0x0411, 0x001c, 0x0113, 0x0113,
		"Buffalo",
		"DUB-P40G HDD",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_INQUIRY ),

/* Submitted by Ernestas Vaiciukevicius <ernisv@gmail.com> */
UNUSUAL_DEV(  0x0419, 0x0100, 0x0100, 0x0100,
		"Samsung Info. Systems America, Inc.",
		"MP3 Player",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

/* Reported by Orgad Shaneh <orgads@gmail.com> */
UNUSUAL_DEV(  0x0419, 0xaace, 0x0100, 0x0100,
		"Samsung", "MP3 Player",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

/* Reported by Christian Leber <christian@leber.de> */
UNUSUAL_DEV(  0x0419, 0xaaf5, 0x0100, 0x0100,
		"TrekStor",
		"i.Beat 115 2.0",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE | US_FL_NOT_LOCKABLE ),

/* Reported by Stefan Werner <dustbln@gmx.de> */
UNUSUAL_DEV(  0x0419, 0xaaf6, 0x0100, 0x0100,
		"TrekStor",
		"i.Beat Joy 2.0",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

/* Reported by Pete Zaitcev <zaitcev@redhat.com>, bz#176584 */
UNUSUAL_DEV(  0x0420, 0x0001, 0x0100, 0x0100,
		"GENERIC", "MP3 PLAYER", /* MyMusix PD-205 on the outside. */
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

/* Reported by Andrew Nayenko <relan@bk.ru> */
UNUSUAL_DEV(  0x0421, 0x0019, 0x0592, 0x0592,
		"Nokia",
		"Nokia 6288",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_MAX_SECTORS_64 ),

/* Reported by Mario Rettig <mariorettig@web.de> */
UNUSUAL_DEV(  0x0421, 0x042e, 0x0100, 0x0100,
		"Nokia",
		"Nokia 3250",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE | US_FL_FIX_CAPACITY ),

/* Reported by <honkkis@gmail.com> */
UNUSUAL_DEV(  0x0421, 0x0433, 0x0100, 0x0100,
		"Nokia",
		"E70",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE | US_FL_FIX_CAPACITY ),

/* Reported by Jon Hart <Jon.Hart@web.de> */
UNUSUAL_DEV(  0x0421, 0x0434, 0x0100, 0x0100,
		"Nokia",
		"E60",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY | US_FL_IGNORE_RESIDUE ),

UNUSUAL_DEV(  0x0421, 0x0444, 0x0100, 0x0100,
		"Nokia",
		"N91",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE | US_FL_FIX_CAPACITY ),

UNUSUAL_DEV(  0x0421, 0x0446, 0x0100, 0x0100,
		"Nokia",
		"N80",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE | US_FL_FIX_CAPACITY ),

/* Reported by Matthew Bloch <matthew@bytemark.co.uk> */
UNUSUAL_DEV(  0x0421, 0x044e, 0x0100, 0x0100,
		"Nokia",
		"E61",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE | US_FL_FIX_CAPACITY ),

/* Reported by Bardur Arantsson <bardur@scientician.net> */
UNUSUAL_DEV(  0x0421, 0x047c, 0x0370, 0x0610,
		"Nokia",
		"6131",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_MAX_SECTORS_64 ),

/* Reported by Manuel Osdoba <manuel.osdoba@tu-ilmenau.de> */
UNUSUAL_DEV( 0x0421, 0x0492, 0x0452, 0x9999,
		"Nokia",
		"Nokia 6233",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_MAX_SECTORS_64 ),

/* Reported by Alex Corcoles <alex@corcoles.net> */
UNUSUAL_DEV(  0x0421, 0x0495, 0x0370, 0x0370,
		"Nokia",
		"6234",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_MAX_SECTORS_64 ),

/* Reported by Olaf Hering <olh@suse.de> from novell bug #105878 */
UNUSUAL_DEV(  0x0424, 0x0fdc, 0x0210, 0x0210,
		"SMSC",
		"FDC GOLD-2.30",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_SINGLE_LUN ),

#ifdef CONFIG_USB_STORAGE_SDDR09
UNUSUAL_DEV(  0x0436, 0x0005, 0x0100, 0x0100,
		"Microtech",
		"CameraMate (DPCM_USB)",
 		US_SC_SCSI, US_PR_DPCM_USB, NULL, 0 ),
#else
UNUSUAL_DEV(  0x0436, 0x0005, 0x0100, 0x0100,
		"Microtech",
		"CameraMate",
		US_SC_SCSI, US_PR_CB, NULL,
		US_FL_SINGLE_LUN ),
#endif

UNUSUAL_DEV(  0x0451, 0x5416, 0x0100, 0x0100,
		"Neuros Audio",
		"USB 2.0 HD 2.5",
		US_SC_DEVICE, US_PR_BULK, NULL,
		US_FL_NEED_OVERRIDE ),

UNUSUAL_DEV(  0x0457, 0x0150, 0x0100, 0x0100,
		"USBest Technology",	/* sold by Transcend */
		"USB Mass Storage Device",
		US_SC_DEVICE, US_PR_DEVICE, NULL, US_FL_NOT_LOCKABLE ),

UNUSUAL_DEV(  0x0457, 0x0151, 0x0100, 0x0100,
		"USB 2.0",
		"Flash Disk",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_NOT_LOCKABLE ),

#ifdef CONFIG_USB_STORAGE_KARMA
UNUSUAL_DEV(  0x045a, 0x5210, 0x0101, 0x0101,
		"Rio",
		"Rio Karma",
		US_SC_SCSI, US_PR_KARMA, rio_karma_init, 0),
#endif

UNUSUAL_DEV(  0x045e, 0xffff, 0x0000, 0x0000,
		"Mitac",
		"GPS",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_MAX_SECTORS_64 ),

UNUSUAL_DEV(  0x046b, 0xff40, 0x0100, 0x0100,
		"AMI",
		"Virtual Floppy",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_NO_WP_DETECT),

/* Patch submitted by Philipp Friedrich <philipp@void.at> */
UNUSUAL_DEV(  0x0482, 0x0100, 0x0100, 0x0100,
		"Kyocera",
		"Finecam S3x",
		US_SC_8070, US_PR_CB, NULL, US_FL_FIX_INQUIRY),

/* Patch submitted by Philipp Friedrich <philipp@void.at> */
UNUSUAL_DEV(  0x0482, 0x0101, 0x0100, 0x0100,
		"Kyocera",
		"Finecam S4",
		US_SC_8070, US_PR_CB, NULL, US_FL_FIX_INQUIRY),

/* Patch submitted by Stephane Galles <stephane.galles@free.fr> */
UNUSUAL_DEV(  0x0482, 0x0103, 0x0100, 0x0100,
		"Kyocera",
		"Finecam S5",
		US_SC_DEVICE, US_PR_DEVICE, NULL, US_FL_FIX_INQUIRY),

/* Patch submitted by Jens Taprogge <jens.taprogge@taprogge.org> */
UNUSUAL_DEV(  0x0482, 0x0107, 0x0100, 0x0100,
		"Kyocera",
		"CONTAX SL300R T*",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY | US_FL_NOT_LOCKABLE),

UNUSUAL_DEV(  0x04a4, 0x0004, 0x0001, 0x0001,
		"Hitachi",
		"DVD-CAM DZ-MV100A Camcorder",
		US_SC_SCSI, US_PR_CB, NULL, US_FL_SINGLE_LUN),

UNUSUAL_DEV(  0x04a5, 0x3010, 0x0100, 0x0100,
		"Tekom Technologies, Inc",
		"300_CAMERA",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

UNUSUAL_DEV(  0x04b0, 0x0301, 0x0010, 0x0010,
		"NIKON",
		"NIKON DSC E2000",
		US_SC_DEVICE, US_PR_DEVICE,NULL,
		US_FL_NOT_LOCKABLE ),

/* Reported by Doug Maxey (dwm@austin.ibm.com) */
UNUSUAL_DEV(  0x04b3, 0x4001, 0x0110, 0x0110,
		"IBM",
		"IBM RSA2",
		US_SC_DEVICE, US_PR_CB, NULL,
		US_FL_MAX_SECTORS_MIN),

#ifdef CONFIG_USB_STORAGE_CYPRESS_ATACB
/* CY7C68300 : support atacb */
UNUSUAL_DEV(  0x04b4, 0x6830, 0x0000, 0x9999,
		"Cypress",
		"Cypress AT2LP",
		US_SC_CYP_ATACB, US_PR_DEVICE, NULL,
		0),

/* CY7C68310 : support atacb and atacb2 */
UNUSUAL_DEV(  0x04b4, 0x6831, 0x0000, 0x9999,
		"Cypress",
		"Cypress ISD-300LP",
		US_SC_CYP_ATACB, US_PR_DEVICE, NULL,
		0),
#endif

UNUSUAL_DEV(  0x04b8, 0x0601, 0x0100, 0x0100,
		"Epson",
		"875DC Storage",
		US_SC_SCSI, US_PR_CB, NULL, US_FL_FIX_INQUIRY),

UNUSUAL_DEV(  0x04b8, 0x0602, 0x0110, 0x0110,
		"Epson",
		"785EPX Storage",
		US_SC_SCSI, US_PR_BULK, NULL, US_FL_SINGLE_LUN),

UNUSUAL_DEV(  0x04cb, 0x0100, 0x0000, 0x2210,
		"Fujifilm",
		"FinePix 1400Zoom",
		US_SC_UFI, US_PR_DEVICE, NULL, US_FL_FIX_INQUIRY | US_FL_SINGLE_LUN),

UNUSUAL_DEV(  0x04ce, 0x0002, 0x0074, 0x0074,
		"ScanLogic",
		"SL11R-IDE",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_INQUIRY),

UNUSUAL_DEV(  0x04da, 0x0901, 0x0100, 0x0200,
		"Panasonic",
		"LS-120 Camera",
		US_SC_UFI, US_PR_DEVICE, NULL, 0),

UNUSUAL_DEV(  0x04da, 0x0d05, 0x0000, 0x0000,
		"Sharp CE-CW05",
		"CD-R/RW Drive",
		US_SC_8070, US_PR_CB, NULL, 0),

/* Reported by Adriaan Penning <a.penning@luon.net> */
UNUSUAL_DEV(  0x04da, 0x2372, 0x0000, 0x9999,
		"Panasonic",
		"DMC-LCx Camera",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY | US_FL_NOT_LOCKABLE ),

/* Reported by Simeon Simeonov <simeonov_2000@yahoo.com> */
UNUSUAL_DEV(  0x04da, 0x2373, 0x0000, 0x9999,
		"LEICA",
		"D-LUX Camera",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY | US_FL_NOT_LOCKABLE ),

UNUSUAL_DEV(  0x04e6, 0x0001, 0x0200, 0x0200,
		"Matshita",
		"LS-120",
		US_SC_8020, US_PR_CB, NULL, 0),

UNUSUAL_DEV(  0x04e6, 0x0002, 0x0100, 0x0100,
		"Shuttle",
		"eUSCSI Bridge",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_euscsi_init, 
		US_FL_SCM_MULT_TARG ),

#ifdef CONFIG_USB_STORAGE_SDDR09
UNUSUAL_DEV(  0x04e6, 0x0003, 0x0000, 0x9999,
		"Sandisk",
		"ImageMate SDDR09",
		US_SC_SCSI, US_PR_EUSB_SDDR09, usb_stor_sddr09_init,
		0),

/* This entry is from Andries.Brouwer@cwi.nl */
UNUSUAL_DEV(  0x04e6, 0x0005, 0x0100, 0x0208,
		"SCM Microsystems",
		"eUSB SmartMedia / CompactFlash Adapter",
		US_SC_SCSI, US_PR_DPCM_USB, usb_stor_sddr09_dpcm_init,
		0),
#else
UNUSUAL_DEV(  0x04e6, 0x0005, 0x0100, 0x0208,
		"SCM Microsystems",
		"eUSB CompactFlash Adapter",
		US_SC_SCSI, US_PR_CB, NULL,
		US_FL_SINGLE_LUN),
#endif

/* Reported by Markus Demleitner <msdemlei@cl.uni-heidelberg.de> */
UNUSUAL_DEV(  0x04e6, 0x0006, 0x0100, 0x0100,
		"SCM Microsystems Inc.",
		"eUSB MMC Adapter",
		US_SC_SCSI, US_PR_CB, NULL,
		US_FL_SINGLE_LUN),

/* Reported by Daniel Nouri <dpunktnpunkt@web.de> */
UNUSUAL_DEV(  0x04e6, 0x0006, 0x0205, 0x0205,
		"Shuttle",
		"eUSB MMC Adapter",
		US_SC_SCSI, US_PR_DEVICE, NULL,
		US_FL_SINGLE_LUN),

UNUSUAL_DEV(  0x04e6, 0x0007, 0x0100, 0x0200,
		"Sony",
		"Hifd",
		US_SC_SCSI, US_PR_CB, NULL,
		US_FL_SINGLE_LUN),

UNUSUAL_DEV(  0x04e6, 0x0009, 0x0200, 0x0200,
		"Shuttle",
		"eUSB ATA/ATAPI Adapter",
		US_SC_8020, US_PR_CB, NULL, 0),

UNUSUAL_DEV(  0x04e6, 0x000a, 0x0200, 0x0200,
		"Shuttle",
		"eUSB CompactFlash Adapter",
		US_SC_8020, US_PR_CB, NULL, 0),

UNUSUAL_DEV(  0x04e6, 0x000B, 0x0100, 0x0100,
		"Shuttle",
		"eUSCSI Bridge",
		US_SC_SCSI, US_PR_BULK, usb_stor_euscsi_init,
		US_FL_SCM_MULT_TARG ), 

UNUSUAL_DEV(  0x04e6, 0x000C, 0x0100, 0x0100,
		"Shuttle",
		"eUSCSI Bridge",
		US_SC_SCSI, US_PR_BULK, usb_stor_euscsi_init,
		US_FL_SCM_MULT_TARG ),

UNUSUAL_DEV(  0x04e6, 0x0101, 0x0200, 0x0200,
		"Shuttle",
		"CD-RW Device",
		US_SC_8020, US_PR_CB, NULL, 0),

#ifdef CONFIG_USB_STORAGE_USBAT
UNUSUAL_DEV(  0x04e6, 0x1010, 0x0000, 0x9999,
		"Shuttle/SCM",
		"USBAT-02",
		US_SC_SCSI, US_PR_USBAT, init_usbat_flash,
		US_FL_SINGLE_LUN),
#endif

/* Reported by Dmitry Khlystov <adminimus@gmail.com> */
UNUSUAL_DEV(  0x04e8, 0x507c, 0x0220, 0x0220,
		"Samsung",
		"YP-U3",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_MAX_SECTORS_64),

UNUSUAL_DEV(  0x04fc, 0x80c2, 0x0100, 0x0100,
		"Kobian Mercury",
		"Binocam DCB-132",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_BULK32),

/* Reported by Bob Sass <rls@vectordb.com> -- only rev 1.33 tested */
UNUSUAL_DEV(  0x050d, 0x0115, 0x0133, 0x0133,
		"Belkin",
		"USB SCSI Adaptor",
		US_SC_SCSI, US_PR_BULK, usb_stor_euscsi_init,
		US_FL_SCM_MULT_TARG ),

UNUSUAL_DEV(  0x0525, 0xa140, 0x0100, 0x0100,
		"Iomega",
		"USB Clik! 40",
		US_SC_8070, US_PR_DEVICE, NULL,
		US_FL_FIX_INQUIRY ),

/* Added by Alan Stern <stern@rowland.harvard.edu> */
COMPLIANT_DEV(0x0525, 0xa4a5, 0x0000, 0x9999,
		"Linux",
		"File-backed Storage Gadget",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_CAPACITY_OK ),

UNUSUAL_DEV(  0x052b, 0x1801, 0x0100, 0x0100,
		"Tekom Technologies, Inc",
		"300_CAMERA",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

UNUSUAL_DEV(  0x052b, 0x1804, 0x0100, 0x0100,
		"Tekom Technologies, Inc",
		"300_CAMERA",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

/* Reported by Iacopo Spalletti <avvisi@spalletti.it> */
UNUSUAL_DEV(  0x052b, 0x1807, 0x0100, 0x0100,
		"Tekom Technologies, Inc",
		"300_CAMERA",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

UNUSUAL_DEV(  0x052b, 0x1905, 0x0100, 0x0100,
		"Tekom Technologies, Inc",
		"400_CAMERA",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

UNUSUAL_DEV(  0x052b, 0x1911, 0x0100, 0x0100,
		"Tekom Technologies, Inc",
		"400_CAMERA",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

UNUSUAL_DEV(  0x054c, 0x0010, 0x0106, 0x0450,
		"Sony",
		"DSC-S30/S70/S75/505V/F505/F707/F717/P8",
		US_SC_SCSI, US_PR_DEVICE, NULL,
		US_FL_SINGLE_LUN | US_FL_NOT_LOCKABLE | US_FL_NO_WP_DETECT ),

UNUSUAL_DEV(  0x054c, 0x0010, 0x0500, 0x0610,
		"Sony",
		"DSC-T1/T5/H5",
		US_SC_8070, US_PR_DEVICE, NULL,
		US_FL_SINGLE_LUN ),


/* Reported by wim@geeks.nl */
UNUSUAL_DEV(  0x054c, 0x0025, 0x0100, 0x0100,
		"Sony",
		"Memorystick NW-MS7",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_SINGLE_LUN ),

#ifdef CONFIG_USB_STORAGE_ISD200
UNUSUAL_DEV(  0x054c, 0x002b, 0x0100, 0x0110,
		"Sony",
		"Portable USB Harddrive V2",
		US_SC_ISD200, US_PR_BULK, isd200_Initialization,
		0 ),
#endif

/* Submitted by Olaf Hering, <olh@suse.de> SuSE Bugzilla #49049 */
UNUSUAL_DEV(  0x054c, 0x002c, 0x0501, 0x2000,
		"Sony",
		"USB Floppy Drive",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_SINGLE_LUN ),

UNUSUAL_DEV(  0x054c, 0x002d, 0x0100, 0x0100,
		"Sony",
		"Memorystick MSAC-US1",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_SINGLE_LUN ),

/* Submitted by Klaus Mueller <k.mueller@intershop.de> */
UNUSUAL_DEV(  0x054c, 0x002e, 0x0106, 0x0310,
		"Sony",
		"Handycam",
		US_SC_SCSI, US_PR_DEVICE, NULL,
		US_FL_SINGLE_LUN ),

/* Submitted by Rajesh Kumble Nayak <nayak@obs-nice.fr> */
UNUSUAL_DEV(  0x054c, 0x002e, 0x0500, 0x0500,
		"Sony",
		"Handycam HC-85",
		US_SC_UFI, US_PR_DEVICE, NULL,
		US_FL_SINGLE_LUN ),

UNUSUAL_DEV(  0x054c, 0x0032, 0x0000, 0x9999,
		"Sony",
		"Memorystick MSC-U01N",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_SINGLE_LUN ),

/* Submitted by Michal Mlotek <mlotek@foobar.pl> */
UNUSUAL_DEV(  0x054c, 0x0058, 0x0000, 0x9999,
		"Sony",
		"PEG N760c Memorystick",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_INQUIRY ),
		
UNUSUAL_DEV(  0x054c, 0x0069, 0x0000, 0x9999,
		"Sony",
		"Memorystick MSC-U03",
		US_SC_UFI, US_PR_CB, NULL,
		US_FL_SINGLE_LUN ),

/* Submitted by Nathan Babb <nathan@lexi.com> */
UNUSUAL_DEV(  0x054c, 0x006d, 0x0000, 0x9999,
		"Sony",
		"PEG Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_INQUIRY ),

/* Submitted by Frank Engel <frankie@cse.unsw.edu.au> */
UNUSUAL_DEV(  0x054c, 0x0099, 0x0000, 0x9999,
		"Sony",
		"PEG Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_INQUIRY ),

/* Submitted by Mike Alborn <malborn@deandra.homeip.net> */
UNUSUAL_DEV(  0x054c, 0x016a, 0x0000, 0x9999,
		"Sony",
		"PEG Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_INQUIRY ),

/* floppy reports multiple luns */
UNUSUAL_DEV(  0x055d, 0x2020, 0x0000, 0x0210,
		"SAMSUNG",
		"SFD-321U [FW 0C]",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_SINGLE_LUN ),

		
UNUSUAL_DEV(  0x057b, 0x0000, 0x0000, 0x0299,
		"Y-E Data",
		"Flashbuster-U",
		US_SC_DEVICE,  US_PR_CB, NULL,
		US_FL_SINGLE_LUN),

UNUSUAL_DEV(  0x057b, 0x0000, 0x0300, 0x9999,
		"Y-E Data",
		"Flashbuster-U",
		US_SC_DEVICE,  US_PR_DEVICE, NULL,
		US_FL_SINGLE_LUN),

UNUSUAL_DEV(  0x057b, 0x0022, 0x0000, 0x9999,
		"Y-E Data",
		"Silicon Media R/W",
		US_SC_DEVICE, US_PR_DEVICE, NULL, 0),

#ifdef CONFIG_USB_STORAGE_ALAUDA
UNUSUAL_DEV(  0x0584, 0x0008, 0x0102, 0x0102,
		"Fujifilm",
		"DPC-R1 (Alauda)",
 		US_SC_SCSI, US_PR_ALAUDA, init_alauda, 0 ),
#endif

/* Reported by RTE <raszilki@yandex.ru> */
UNUSUAL_DEV(  0x058f, 0x6387, 0x0141, 0x0141,
		"JetFlash",
		"TS1GJF2A/120",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_MAX_SECTORS_64 ),

/* Fabrizio Fellini <fello@libero.it> */
UNUSUAL_DEV(  0x0595, 0x4343, 0x0000, 0x2210,
		"Fujifilm",
		"Digital Camera EX-20 DSC",
		US_SC_8070, US_PR_DEVICE, NULL, 0 ),

UNUSUAL_DEV(  0x059b, 0x0001, 0x0100, 0x0100,
		"Iomega",
		"ZIP 100",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_SINGLE_LUN ),

/* Reported by <Hendryk.Pfeiffer@gmx.de> */
UNUSUAL_DEV(  0x059f, 0x0643, 0x0000, 0x0000,
		"LaCie",
		"DVD+-RW",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_GO_SLOW ),

UNUSUAL_DEV(  0x05ab, 0x0060, 0x1104, 0x1110,
		"In-System",
		"PyroGate External CD-ROM Enclosure (FCD-523)",
		US_SC_SCSI, US_PR_BULK, NULL,
		US_FL_NEED_OVERRIDE ),

#ifdef CONFIG_USB_STORAGE_ISD200
UNUSUAL_DEV(  0x05ab, 0x0031, 0x0100, 0x0110,
		"In-System",
		"USB/IDE Bridge (ATA/ATAPI)",
		US_SC_ISD200, US_PR_BULK, isd200_Initialization,
		0 ),

UNUSUAL_DEV(  0x05ab, 0x0301, 0x0100, 0x0110,
		"In-System",
		"Portable USB Harddrive V2",
		US_SC_ISD200, US_PR_BULK, isd200_Initialization,
		0 ),

UNUSUAL_DEV(  0x05ab, 0x0351, 0x0100, 0x0110,
		"In-System",
		"Portable USB Harddrive V2",
		US_SC_ISD200, US_PR_BULK, isd200_Initialization,
		0 ),

UNUSUAL_DEV(  0x05ab, 0x5701, 0x0100, 0x0110,
		"In-System",
		"USB Storage Adapter V2",
		US_SC_ISD200, US_PR_BULK, isd200_Initialization,
		0 ),
#endif

UNUSUAL_DEV( 0x05ac, 0x1202, 0x0000, 0x9999,
		"Apple",
		"iPod",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY ),

/* Reported by Avi Kivity <avi@argo.co.il> */
UNUSUAL_DEV( 0x05ac, 0x1203, 0x0000, 0x9999,
		"Apple",
		"iPod",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY ),

UNUSUAL_DEV( 0x05ac, 0x1204, 0x0000, 0x9999,
		"Apple",
		"iPod",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY | US_FL_NOT_LOCKABLE ),

UNUSUAL_DEV( 0x05ac, 0x1205, 0x0000, 0x9999,
		"Apple",
		"iPod",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY ),

UNUSUAL_DEV( 0x05ac, 0x120a, 0x0000, 0x9999,
		"Apple",
		"iPod",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY ),


/* Globetrotter HSDPA; mass storage shows up as Qualcomm for vendor */
UNUSUAL_DEV(  0x05c6, 0x1000, 0x0000, 0x9999,
		"Option N.V.",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, option_ms_init,
		0),

#ifdef CONFIG_USB_STORAGE_JUMPSHOT
UNUSUAL_DEV(  0x05dc, 0x0001, 0x0000, 0x0001,
		"Lexar",
		"Jumpshot USB CF Reader",
		US_SC_SCSI, US_PR_JUMPSHOT, NULL,
		US_FL_NEED_OVERRIDE ),
#endif

/* Reported by Blake Matheny <bmatheny@purdue.edu> */
UNUSUAL_DEV(  0x05dc, 0xb002, 0x0000, 0x0113,
		"Lexar",
		"USB CF Reader",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_INQUIRY ),

UNUSUAL_DEV(  0x05e3, 0x0701, 0x0000, 0xffff,
		"Genesys Logic",
		"USB to IDE Optical",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_GO_SLOW | US_FL_MAX_SECTORS_64 | US_FL_IGNORE_RESIDUE ),

UNUSUAL_DEV(  0x05e3, 0x0702, 0x0000, 0xffff,
		"Genesys Logic",
		"USB to IDE Disk",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_GO_SLOW | US_FL_MAX_SECTORS_64 | US_FL_IGNORE_RESIDUE ),

/* Reported by Ben Efros <ben@pc-doctor.com> */
UNUSUAL_DEV(  0x05e3, 0x0723, 0x9451, 0x9451,
		"Genesys Logic",
		"USB to SATA",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_SANE_SENSE ),

UNUSUAL_DEV(  0x0636, 0x0003, 0x0000, 0x9999,
		"Vivitar",
		"Vivicam 35Xx",
		US_SC_SCSI, US_PR_BULK, NULL,
		US_FL_FIX_INQUIRY ),

UNUSUAL_DEV(  0x0644, 0x0000, 0x0100, 0x0100,
		"TEAC",
		"Floppy Drive",
		US_SC_UFI, US_PR_CB, NULL, 0 ),

#ifdef CONFIG_USB_STORAGE_SDDR09
UNUSUAL_DEV(  0x066b, 0x0105, 0x0100, 0x0100,
		"Olympus",
		"Camedia MAUSB-2",
		US_SC_SCSI, US_PR_EUSB_SDDR09, usb_stor_sddr09_init,
		0),
#endif

/* Reported by Darsen Lu <darsen@micro.ee.nthu.edu.tw> */
UNUSUAL_DEV( 0x066f, 0x8000, 0x0001, 0x0001,
		"SigmaTel",
		"USBMSC Audio Player",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY ),

/* Reported by Richard -=[]=- <micro_flyer@hotmail.com> */
UNUSUAL_DEV( 0x067b, 0x2507, 0x0001, 0x0100,
		"Prolific Technology Inc.",
		"Mass Storage Device",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY | US_FL_GO_SLOW ),

/* Reported by Alex Butcher <alex.butcher@assursys.co.uk> */
UNUSUAL_DEV( 0x067b, 0x3507, 0x0001, 0x0101,
		"Prolific Technology Inc.",
		"ATAPI-6 Bridge Controller",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY | US_FL_GO_SLOW ),

/* Submitted by Benny Sjostrand <benny@hostmobility.com> */
UNUSUAL_DEV( 0x0686, 0x4011, 0x0001, 0x0001,
		"Minolta",
		"Dimage F300",
		US_SC_SCSI, US_PR_BULK, NULL, 0 ),

/* Reported by Miguel A. Fosas <amn3s1a@ono.com> */
UNUSUAL_DEV(  0x0686, 0x4017, 0x0001, 0x0001,
		"Minolta",
		"DIMAGE E223",
		US_SC_SCSI, US_PR_DEVICE, NULL, 0 ),

UNUSUAL_DEV(  0x0693, 0x0005, 0x0100, 0x0100,
		"Hagiwara",
		"Flashgate",
		US_SC_SCSI, US_PR_BULK, NULL, 0 ),

/* Reported by David Hamilton <niftimusmaximus@lycos.com> */
UNUSUAL_DEV(  0x069b, 0x3004, 0x0001, 0x0001,
		"Thomson Multimedia Inc.",
		"RCA RD1080 MP3 Player",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY ),

/* Reported by Adrian Pilchowiec <adi1981@epf.pl> */
UNUSUAL_DEV(  0x071b, 0x3203, 0x0000, 0x0000,
		"RockChip",
		"MP3",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_NO_WP_DETECT | US_FL_MAX_SECTORS_64),

UNUSUAL_DEV(  0x071b, 0x32bb, 0x0000, 0x0000,
		"RockChip",
		"MTP",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_NO_WP_DETECT | US_FL_MAX_SECTORS_64),

UNUSUAL_DEV(  0x071b, 0x3203, 0x0100, 0x0100,
		"RockChip",
		"ROCK MP3",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_MAX_SECTORS_64),

/* Reported by Olivier Blondeau <zeitoun@gmail.com> */
UNUSUAL_DEV(  0x0727, 0x0306, 0x0100, 0x0100,
		"ATMEL",
		"SND1 Storage",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE),

/* Submitted by Roman Hodek <roman@hodek.net> */
UNUSUAL_DEV(  0x0781, 0x0001, 0x0200, 0x0200,
		"Sandisk",
		"ImageMate SDDR-05a",
		US_SC_SCSI, US_PR_CB, NULL,
		US_FL_SINGLE_LUN ),

UNUSUAL_DEV(  0x0781, 0x0002, 0x0009, 0x0009,
		"SanDisk Corporation",
		"ImageMate CompactFlash USB",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY ),

#ifdef CONFIG_USB_STORAGE_USBAT
UNUSUAL_DEV(  0x0781, 0x0005, 0x0005, 0x0005,
		"Sandisk",
		"ImageMate SDDR-05b",
		US_SC_SCSI, US_PR_USBAT, init_usbat_flash,
		US_FL_SINGLE_LUN ),
#endif

UNUSUAL_DEV(  0x0781, 0x0100, 0x0100, 0x0100,
		"Sandisk",
		"ImageMate SDDR-12",
		US_SC_SCSI, US_PR_CB, NULL,
		US_FL_SINGLE_LUN ),

#ifdef CONFIG_USB_STORAGE_SDDR09
UNUSUAL_DEV(  0x0781, 0x0200, 0x0000, 0x9999,
		"Sandisk",
		"ImageMate SDDR-09",
		US_SC_SCSI, US_PR_EUSB_SDDR09, usb_stor_sddr09_init,
		0),
#endif

#ifdef CONFIG_USB_STORAGE_FREECOM
UNUSUAL_DEV(  0x07ab, 0xfc01, 0x0000, 0x9999,
		"Freecom",
		"USB-IDE",
		US_SC_QIC, US_PR_FREECOM, freecom_init, 0),
#endif

/* Reported by Eero Volotinen <eero@ping-viini.org> */
UNUSUAL_DEV(  0x07ab, 0xfccd, 0x0000, 0x9999,
		"Freecom Technologies",
		"FHD-Classic",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY),

UNUSUAL_DEV(  0x07af, 0x0004, 0x0100, 0x0133,
		"Microtech",
		"USB-SCSI-DB25",
		US_SC_SCSI, US_PR_BULK, usb_stor_euscsi_init,
		US_FL_SCM_MULT_TARG ), 

UNUSUAL_DEV(  0x07af, 0x0005, 0x0100, 0x0100,
		"Microtech",
		"USB-SCSI-HD50",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_euscsi_init,
		US_FL_SCM_MULT_TARG ),

#ifdef CONFIG_USB_STORAGE_SDDR09
UNUSUAL_DEV(  0x07af, 0x0006, 0x0100, 0x0100,
		"Microtech",
		"CameraMate (DPCM_USB)",
 		US_SC_SCSI, US_PR_DPCM_USB, NULL, 0 ),
#else
UNUSUAL_DEV(  0x07af, 0x0006, 0x0100, 0x0100,
		"Microtech",
		"CameraMate",
		US_SC_SCSI, US_PR_CB, NULL,
		US_FL_SINGLE_LUN ),
#endif

#ifdef CONFIG_USB_STORAGE_ALAUDA
UNUSUAL_DEV(  0x07b4, 0x010a, 0x0102, 0x0102,
		"Olympus",
		"MAUSB-10 (Alauda)",
 		US_SC_SCSI, US_PR_ALAUDA, init_alauda, 0 ),
#endif

#ifdef CONFIG_USB_STORAGE_DATAFAB
UNUSUAL_DEV(  0x07c4, 0xa000, 0x0000, 0x0015,
		"Datafab",
		"MDCFE-B USB CF Reader",
		US_SC_SCSI, US_PR_DATAFAB, NULL,
		0 ),

UNUSUAL_DEV( 0x07c4, 0xa001, 0x0000, 0xffff,
		"SIIG/Datafab",
		"SIIG/Datafab Memory Stick+CF Reader/Writer",
		US_SC_SCSI, US_PR_DATAFAB, NULL,
		0 ),

/* Reported by Josef Reisinger <josef.reisinger@netcologne.de> */
UNUSUAL_DEV( 0x07c4, 0xa002, 0x0000, 0xffff,
		"Datafab/Unknown",
		"MD2/MD3 Disk enclosure",
		US_SC_SCSI, US_PR_DATAFAB, NULL,
		US_FL_SINGLE_LUN ),

UNUSUAL_DEV( 0x07c4, 0xa003, 0x0000, 0xffff,
		"Datafab/Unknown",
		"Datafab-based Reader",
		US_SC_SCSI, US_PR_DATAFAB, NULL,
		0 ),

UNUSUAL_DEV( 0x07c4, 0xa004, 0x0000, 0xffff,
		"Datafab/Unknown",
		"Datafab-based Reader",
		US_SC_SCSI, US_PR_DATAFAB, NULL,
		0 ),

UNUSUAL_DEV( 0x07c4, 0xa005, 0x0000, 0xffff,
		"PNY/Datafab",
		"PNY/Datafab CF+SM Reader",
		US_SC_SCSI, US_PR_DATAFAB, NULL,
		0 ),

UNUSUAL_DEV( 0x07c4, 0xa006, 0x0000, 0xffff,
		"Simple Tech/Datafab",
		"Simple Tech/Datafab CF+SM Reader",
		US_SC_SCSI, US_PR_DATAFAB, NULL,
		0 ),
#endif
		
#ifdef CONFIG_USB_STORAGE_SDDR55
/* Contributed by Peter Waechtler */
UNUSUAL_DEV( 0x07c4, 0xa103, 0x0000, 0x9999,
		"Datafab",
		"MDSM-B reader",
		US_SC_SCSI, US_PR_SDDR55, NULL,
		US_FL_FIX_INQUIRY ),
#endif

#ifdef CONFIG_USB_STORAGE_DATAFAB
/* Submitted by Olaf Hering <olh@suse.de> */
UNUSUAL_DEV(  0x07c4, 0xa109, 0x0000, 0xffff,
		"Datafab Systems, Inc.",
		"USB to CF + SM Combo (LC1)",
		US_SC_SCSI, US_PR_DATAFAB, NULL,
		0 ),
#endif
#ifdef CONFIG_USB_STORAGE_SDDR55
/* SM part - aeb <Andries.Brouwer@cwi.nl> */
UNUSUAL_DEV(  0x07c4, 0xa109, 0x0000, 0xffff,
		"Datafab Systems, Inc.",
		"USB to CF + SM Combo (LC1)",
		US_SC_SCSI, US_PR_SDDR55, NULL,
		US_FL_SINGLE_LUN ),
#endif

#ifdef CONFIG_USB_STORAGE_DATAFAB
UNUSUAL_DEV(  0x07c4, 0xa10b, 0x0000, 0xffff,
		"DataFab Systems Inc.",
		"USB CF+MS",
		US_SC_SCSI, US_PR_DATAFAB, NULL,
		0 ),

#endif

UNUSUAL_DEV(  0x07c4, 0xa400, 0x0000, 0xffff,
		"Datafab",
		"KECF-USB",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_INQUIRY | US_FL_FIX_CAPACITY ),

/* Reported by Rauch Wolke <rauchwolke@gmx.net> */
UNUSUAL_DEV(  0x07c4, 0xa4a5, 0x0000, 0xffff,
		"Simple Tech/Datafab",
		"CF+SM Reader",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

UNUSUAL_DEV( 0x07cf, 0x1001, 0x1000, 0x9999,
		"Casio",
		"QV DigitalCamera",
		US_SC_8070, US_PR_CB, NULL,
		US_FL_NEED_OVERRIDE | US_FL_FIX_INQUIRY ),

/* Submitted by Hartmut Wahl <hwahl@hwahl.de>*/
UNUSUAL_DEV( 0x0839, 0x000a, 0x0001, 0x0001,
		"Samsung",
		"Digimax 410",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_INQUIRY),

/* Reported by Luciano Rocha <luciano@eurotux.com> */
UNUSUAL_DEV( 0x0840, 0x0082, 0x0001, 0x0001,
		"Argosy",
		"Storage",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY),

/* Reported and patched by Nguyen Anh Quynh <aquynh@gmail.com> */
UNUSUAL_DEV( 0x0840, 0x0084, 0x0001, 0x0001,
		"Argosy",
		"Storage",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY),

/* Reported by Martijn Hijdra <martijn.hijdra@gmail.com> */
UNUSUAL_DEV( 0x0840, 0x0085, 0x0001, 0x0001,
		"Argosy",
		"Storage",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY),


UNUSUAL_DEV(  0x084d, 0x0011, 0x0110, 0x0110,
		"Grandtech",
		"DC2MEGA",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_BULK32),

UNUSUAL_DEV(  0x0851, 0x1543, 0x0200, 0x0200,
		"PanDigital",
		"Photo Frame",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_NOT_LOCKABLE),

/* Submitted by Jan De Luyck <lkml@kcore.org> */
UNUSUAL_DEV(  0x08bd, 0x1100, 0x0000, 0x0000,
		"CITIZEN",
		"X1DE-USB",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_SINGLE_LUN),

UNUSUAL_DEV(  0x08ca, 0x3103, 0x0100, 0x0100,
		"AIPTEK",
		"Aiptek USB Keychain MP3 Player",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE),

UNUSUAL_DEV(  0x090a, 0x1001, 0x0100, 0x0100,
		"Trumpion",
		"t33520 USB Flash Card Controller",
		US_SC_DEVICE, US_PR_BULK, NULL,
		US_FL_NEED_OVERRIDE ),

UNUSUAL_DEV(  0x090a, 0x1050, 0x0100, 0x0100,
		"Trumpion Microelectronics, Inc.",
		"33520 USB Digital Voice Recorder",
		US_SC_UFI, US_PR_DEVICE, NULL,
		0),

/* Trumpion Microelectronics MP3 player (felipe_alfaro@linuxmail.org) */
UNUSUAL_DEV( 0x090a, 0x1200, 0x0000, 0x9999,
		"Trumpion",
		"MP3 player",
		US_SC_RBC, US_PR_BULK, NULL,
		0 ),

/* aeb */
UNUSUAL_DEV( 0x090c, 0x1132, 0x0000, 0xffff,
		"Feiya",
		"5-in-1 Card Reader",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY ),

UNUSUAL_DEV( 0x0a17, 0x0004, 0x1000, 0x1000,
		"Pentax",
		"Optio 2/3/400",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_INQUIRY ),

UNUSUAL_DEV( 0x0ace, 0x2011, 0x0101, 0x0101,
		"ZyXEL",
		"G-220F USB-WLAN Install",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_DEVICE ),

UNUSUAL_DEV( 0x0ace, 0x20ff, 0x0101, 0x0101,
		"SiteCom",
		"WL-117 USB-WLAN Install",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_DEVICE ),


/* iCON 225 */
UNUSUAL_DEV(  0x0af0, 0x6971, 0x0000, 0x9999,
		"Option N.V.",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, option_ms_init,
		0),

UNUSUAL_DEV( 0x0af0, 0x7401, 0x0000, 0x0000,
		"Option",
		"GI 0401 SD-Card",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		0 ),

UNUSUAL_DEV( 0x0af0, 0x7501, 0x0000, 0x0000,
		"Option",
		"GI 0431 SD-Card",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		0 ),

/* Reported by Ben Efros <ben@pc-doctor.com> */
UNUSUAL_DEV( 0x0bc2, 0x3010, 0x0000, 0x0000,
		"Seagate",
		"FreeAgent Pro",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_SANE_SENSE ),

#ifdef CONFIG_USB_STORAGE_ISD200
UNUSUAL_DEV(  0x0bf6, 0xa001, 0x0100, 0x0110,
		"ATI",
		"USB Cable 205",
		US_SC_ISD200, US_PR_BULK, isd200_Initialization,
		0 ),
#endif

#ifdef CONFIG_USB_STORAGE_DATAFAB
UNUSUAL_DEV( 0x0c0b, 0xa109, 0x0000, 0xffff,
		"Acomdata",
		"CF",
		US_SC_SCSI, US_PR_DATAFAB, NULL,
		US_FL_SINGLE_LUN ),
#endif
#ifdef CONFIG_USB_STORAGE_SDDR55
UNUSUAL_DEV( 0x0c0b, 0xa109, 0x0000, 0xffff,
		"Acomdata",
		"SM",
		US_SC_SCSI, US_PR_SDDR55, NULL,
		US_FL_SINGLE_LUN ),
#endif

UNUSUAL_DEV(  0x0d49, 0x7310, 0x0000, 0x9999,
		"Maxtor",
		"USB to SATA",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_SANE_SENSE),

UNUSUAL_DEV( 0x0c45, 0x1060, 0x0100, 0x0100,
		"Unknown",
		"Unknown",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_SINGLE_LUN ),

#ifdef CONFIG_USB_STORAGE_ONETOUCH
	UNUSUAL_DEV(  0x0d49, 0x7000, 0x0000, 0x9999,
			"Maxtor",
			"OneTouch External Harddrive",
			US_SC_DEVICE, US_PR_DEVICE, onetouch_connect_input,
			0),
	UNUSUAL_DEV(  0x0d49, 0x7010, 0x0000, 0x9999,
			"Maxtor",
			"OneTouch External Harddrive",
			US_SC_DEVICE, US_PR_DEVICE, onetouch_connect_input,
			0),
#endif

/* Submitted by Joris Struyve <joris@struyve.be> */
UNUSUAL_DEV( 0x0d96, 0x410a, 0x0001, 0xffff,
		"Medion",
		"MD 7425",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_INQUIRY),

UNUSUAL_DEV(  0x0d96, 0x5200, 0x0001, 0x0200,
		"Jenoptik",
		"JD 5200 z3",
		US_SC_DEVICE, US_PR_DEVICE, NULL, US_FL_FIX_INQUIRY),

/* Reported by  Jason Johnston <killean@shaw.ca> */
UNUSUAL_DEV(  0x0dc4, 0x0073, 0x0000, 0x0000,
		"Macpower Technology Co.LTD.",
		"USB 2.0 3.5\" DEVICE",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY),

UNUSUAL_DEV( 0x0dd8, 0x1060, 0x0000, 0xffff,
		"Netac",
		"USB-CF-Card",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_INQUIRY ),

UNUSUAL_DEV( 0x0dd8, 0xd202, 0x0000, 0x9999,
		"Netac",
		"USB Flash Disk",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),


UNUSUAL_DEV( 0x0dda, 0x0001, 0x0012, 0x0012,
		"WINWARD",
		"Music Disk",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

/* Reported by Ian McConnell <ian at emit.demon.co.uk> */
UNUSUAL_DEV(  0x0dda, 0x0301, 0x0012, 0x0012,
		"PNP_MP3",
		"PNP_MP3 PLAYER",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

/* Reported by Jim McCloskey <mcclosk@ucsc.edu> */
UNUSUAL_DEV( 0x0e21, 0x0520, 0x0100, 0x0100,
		"Cowon Systems",
		"iAUDIO M5",
		US_SC_DEVICE, US_PR_BULK, NULL,
		US_FL_NEED_OVERRIDE ),

/* Submitted by Antoine Mairesse <antoine.mairesse@free.fr> */
UNUSUAL_DEV( 0x0ed1, 0x6660, 0x0100, 0x0300,
		"USB",
		"Solid state disk",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_INQUIRY ),

UNUSUAL_DEV(  0x0ea0, 0x2168, 0x0110, 0x0110,
		"Ours Technology",
		"Flash Disk",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

/* Reported by Rastislav Stanik <rs_kernel@yahoo.com> */
UNUSUAL_DEV(  0x0ea0, 0x6828, 0x0110, 0x0110,
		"USB",
		"Flash Disk",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

UNUSUAL_DEV(  0x0ed1, 0x7636, 0x0103, 0x0103,
		"Typhoon",
		"My DJ 1820",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE | US_FL_GO_SLOW | US_FL_MAX_SECTORS_64),

UNUSUAL_DEV(  0x0f19, 0x0103, 0x0100, 0x0100,
		"Oracom Co., Ltd",
		"ORC-200M",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

UNUSUAL_DEV(  0x0f19, 0x0105, 0x0100, 0x0100,
		"C-MEX",
		"A-VOX",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

#ifdef CONFIG_USB_BERRY_CHARGE
UNUSUAL_DEV(  0x0fca, 0x0006, 0x0001, 0x0001,
		"RIM",
		"Blackberry Pearl",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_DEVICE ),
#endif

/* Reported by Michael Stattmann <michael@stattmann.com> */
UNUSUAL_DEV(  0x0fce, 0xd008, 0x0000, 0x0000,
		"Sony Ericsson",
		"V800-Vodafone 802",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_NO_WP_DETECT ),

/* Reported by The Solutor <thesolutor@gmail.com> */
UNUSUAL_DEV(  0x0fce, 0xd0e1, 0x0000, 0x0000,
		"Sony Ericsson",
		"MD400",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_DEVICE),

UNUSUAL_DEV(  0x0fce, 0xe030, 0x0000, 0x0000,
		"Sony Ericsson",
		"P990i",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY | US_FL_IGNORE_RESIDUE ),

/* Reported by Emmanuel Vasilakis <evas@forthnet.gr> */
UNUSUAL_DEV(  0x0fce, 0xe031, 0x0000, 0x0000,
		"Sony Ericsson",
		"M600i",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE | US_FL_FIX_CAPACITY ),

/* Reported by Ricardo Barberis <ricardo@dattatec.com> */
UNUSUAL_DEV(  0x0fce, 0xe092, 0x0000, 0x0000,
		"Sony Ericsson",
		"P1i",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

UNUSUAL_DEV(  0x1019, 0x0c55, 0x0000, 0x0110,
		"Desknote",
		"UCR-61S2B",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_ucr61s2b_init,
		0 ),

UNUSUAL_DEV(  0x1058, 0x0704, 0x0000, 0x9999,
		"Western Digital",
		"External HDD",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_SANE_SENSE),

UNUSUAL_DEV(  0x10d6, 0x2200, 0x0100, 0x0100,
		"Actions Semiconductor",
		"Mtp device",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		0),

UNUSUAL_DEV(  0x1199, 0x0fff, 0x0000, 0x9999,
		"Sierra Wireless",
		"USB MMC Storage",
		US_SC_DEVICE, US_PR_DEVICE, sierra_ms_init,
		0),

UNUSUAL_DEV(  0x1210, 0x0003, 0x0100, 0x0100,
		"Digitech HMG",
		"DigiTech Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

UNUSUAL_DEV(  0x12d1, 0x1001, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1003, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1004, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1401, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1402, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1403, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1404, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1405, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1406, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1407, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1408, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1409, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x140A, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x140B, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x140C, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x140D, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x140E, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x140F, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1410, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1411, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1412, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1413, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1414, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1415, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1416, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1417, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1418, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1419, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x141A, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x141B, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x141C, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x141D, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x141E, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x141F, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1420, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1421, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1422, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1423, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1424, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1425, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1426, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1427, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1428, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1429, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x142A, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x142B, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x142C, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x142D, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x142E, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x142F, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1430, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1431, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1432, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1433, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1434, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1435, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1436, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1437, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1438, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x1439, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x143A, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x143B, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x143C, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x143D, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x143E, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),
UNUSUAL_DEV(  0x12d1, 0x143F, 0x0000, 0x0000,
		"HUAWEI MOBILE",
		"Mass Storage",
		US_SC_DEVICE, US_PR_DEVICE, usb_stor_huawei_e220_init,
		0),

/* Reported by Vilius Bilinkevicius <vilisas AT xxx DOT lt) */
UNUSUAL_DEV(  0x132b, 0x000b, 0x0001, 0x0001,
		"Minolta",
		"Dimage Z10",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		0 ),

/* Reported by Kotrla Vitezslav <kotrla@ceb.cz> */
UNUSUAL_DEV(  0x1370, 0x6828, 0x0110, 0x0110,
		"SWISSBIT",
		"Black Silver",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

/* Reported by Francesco Foresti <frafore@tiscali.it> */
UNUSUAL_DEV(  0x14cd, 0x6600, 0x0201, 0x0201,
		"Super Top",
		"IDE DEVICE",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

UNUSUAL_DEV(  0x152d, 0x2329, 0x0100, 0x0100,
		"JMicron",
		"USB to ATA/ATAPI Bridge",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE | US_FL_SANE_SENSE ),

UNUSUAL_DEV(  0x1652, 0x6600, 0x0201, 0x0201,
		"Teac",
		"HD-35PUK-B",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

UNUSUAL_DEV( 0x2116, 0x0320, 0x0001, 0x0001,
		"ST",
		"2A",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY),

UNUSUAL_DEV(  0x22b8, 0x3010, 0x0001, 0x0001,
		"Motorola",
		"RAZR V3x",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY | US_FL_IGNORE_RESIDUE ),

UNUSUAL_DEV(  0x22b8, 0x6426, 0x0101, 0x0101,
		"Motorola",
		"MSnc.",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_INQUIRY | US_FL_FIX_CAPACITY | US_FL_BULK_IGNORE_TAG),

/* Reported by Radovan Garabik <garabik@kassiopeia.juls.savba.sk> */
UNUSUAL_DEV(  0x2735, 0x100b, 0x0000, 0x9999,
		"MPIO",
		"HS200",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_GO_SLOW ),

/* Reported by Rohan Hart <rohan.hart17@gmail.com> */
UNUSUAL_DEV(  0x2770, 0x915d, 0x0010, 0x0010,
		"INTOVA",
		"Pixtreme",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_FIX_CAPACITY ),

UNUSUAL_DEV(  0x3340, 0xffff, 0x0000, 0x0000,
		"Mitac",
		"Mio DigiWalker USB Sync",
		US_SC_DEVICE,US_PR_DEVICE,NULL,
		US_FL_MAX_SECTORS_64 ),

/* Reported by Andrey Rahmatullin <wrar@altlinux.org> */
UNUSUAL_DEV(  0x4102, 0x1020, 0x0100,  0x0100,
		"iRiver",
		"MP3 T10",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_IGNORE_RESIDUE ),

UNUSUAL_DEV(  0x4146, 0xba01, 0x0100, 0x0100,
		"Iomega",
		"Micro Mini 1GB",
		US_SC_DEVICE, US_PR_DEVICE, NULL, US_FL_NOT_LOCKABLE ),

#ifdef CONFIG_USB_STORAGE_SDDR55
UNUSUAL_DEV(  0x55aa, 0xa103, 0x0000, 0x9999, 
		"Sandisk",
		"ImageMate SDDR55",
		US_SC_SCSI, US_PR_SDDR55, NULL,
		US_FL_SINGLE_LUN),
#endif

/* Reported by Andrew Simmons <andrew.simmons@gmail.com> */
UNUSUAL_DEV(  0xed06, 0x4500, 0x0001, 0x0001,
		"DataStor",
		"USB4500 FW1.04",
		US_SC_DEVICE, US_PR_DEVICE, NULL,
		US_FL_CAPACITY_HEURISTICS),

/* Control/Bulk transport for all SubClass values */
USUAL_DEV(US_SC_RBC, US_PR_CB, USB_US_TYPE_STOR),
USUAL_DEV(US_SC_8020, US_PR_CB, USB_US_TYPE_STOR),
USUAL_DEV(US_SC_QIC, US_PR_CB, USB_US_TYPE_STOR),
USUAL_DEV(US_SC_UFI, US_PR_CB, USB_US_TYPE_STOR),
USUAL_DEV(US_SC_8070, US_PR_CB, USB_US_TYPE_STOR),
USUAL_DEV(US_SC_SCSI, US_PR_CB, USB_US_TYPE_STOR),

/* Control/Bulk/Interrupt transport for all SubClass values */
USUAL_DEV(US_SC_RBC, US_PR_CBI, USB_US_TYPE_STOR),
USUAL_DEV(US_SC_8020, US_PR_CBI, USB_US_TYPE_STOR),
USUAL_DEV(US_SC_QIC, US_PR_CBI, USB_US_TYPE_STOR),
USUAL_DEV(US_SC_UFI, US_PR_CBI, USB_US_TYPE_STOR),
USUAL_DEV(US_SC_8070, US_PR_CBI, USB_US_TYPE_STOR),
USUAL_DEV(US_SC_SCSI, US_PR_CBI, USB_US_TYPE_STOR),

/* Bulk-only transport for all SubClass values */
USUAL_DEV(US_SC_RBC, US_PR_BULK, USB_US_TYPE_STOR),
USUAL_DEV(US_SC_8020, US_PR_BULK, USB_US_TYPE_STOR),
USUAL_DEV(US_SC_QIC, US_PR_BULK, USB_US_TYPE_STOR),
USUAL_DEV(US_SC_UFI, US_PR_BULK, USB_US_TYPE_STOR),
USUAL_DEV(US_SC_8070, US_PR_BULK, USB_US_TYPE_STOR),
USUAL_DEV(US_SC_SCSI, US_PR_BULK, 0),
