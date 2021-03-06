


#include <acpi/acpi.h>
#include "accommon.h"
#include "actables.h"

#define _COMPONENT          ACPI_TABLES
ACPI_MODULE_NAME("tbfadt")

/* Local prototypes */
static inline void
acpi_tb_init_generic_address(struct acpi_generic_address *generic_address,
			     u8 space_id, u8 byte_width, u64 address);

static void acpi_tb_convert_fadt(void);

static void acpi_tb_validate_fadt(void);

/* Table for conversion of FADT to common internal format and FADT validation */

typedef struct acpi_fadt_info {
	char *name;
	u8 address64;
	u8 address32;
	u8 length;
	u8 default_length;
	u8 type;

} acpi_fadt_info;

#define ACPI_FADT_REQUIRED          1
#define ACPI_FADT_SEPARATE_LENGTH   2

static struct acpi_fadt_info fadt_info_table[] = {
	{"Pm1aEventBlock",
	 ACPI_FADT_OFFSET(xpm1a_event_block),
	 ACPI_FADT_OFFSET(pm1a_event_block),
	 ACPI_FADT_OFFSET(pm1_event_length),
	 ACPI_PM1_REGISTER_WIDTH * 2,	/* Enable + Status register */
	 ACPI_FADT_REQUIRED},

	{"Pm1bEventBlock",
	 ACPI_FADT_OFFSET(xpm1b_event_block),
	 ACPI_FADT_OFFSET(pm1b_event_block),
	 ACPI_FADT_OFFSET(pm1_event_length),
	 ACPI_PM1_REGISTER_WIDTH * 2,	/* Enable + Status register */
	 0},

	{"Pm1aControlBlock",
	 ACPI_FADT_OFFSET(xpm1a_control_block),
	 ACPI_FADT_OFFSET(pm1a_control_block),
	 ACPI_FADT_OFFSET(pm1_control_length),
	 ACPI_PM1_REGISTER_WIDTH,
	 ACPI_FADT_REQUIRED},

	{"Pm1bControlBlock",
	 ACPI_FADT_OFFSET(xpm1b_control_block),
	 ACPI_FADT_OFFSET(pm1b_control_block),
	 ACPI_FADT_OFFSET(pm1_control_length),
	 ACPI_PM1_REGISTER_WIDTH,
	 0},

	{"Pm2ControlBlock",
	 ACPI_FADT_OFFSET(xpm2_control_block),
	 ACPI_FADT_OFFSET(pm2_control_block),
	 ACPI_FADT_OFFSET(pm2_control_length),
	 ACPI_PM2_REGISTER_WIDTH,
	 ACPI_FADT_SEPARATE_LENGTH},

	{"PmTimerBlock",
	 ACPI_FADT_OFFSET(xpm_timer_block),
	 ACPI_FADT_OFFSET(pm_timer_block),
	 ACPI_FADT_OFFSET(pm_timer_length),
	 ACPI_PM_TIMER_WIDTH,
	 ACPI_FADT_REQUIRED},

	{"Gpe0Block",
	 ACPI_FADT_OFFSET(xgpe0_block),
	 ACPI_FADT_OFFSET(gpe0_block),
	 ACPI_FADT_OFFSET(gpe0_block_length),
	 0,
	 ACPI_FADT_SEPARATE_LENGTH},

	{"Gpe1Block",
	 ACPI_FADT_OFFSET(xgpe1_block),
	 ACPI_FADT_OFFSET(gpe1_block),
	 ACPI_FADT_OFFSET(gpe1_block_length),
	 0,
	 ACPI_FADT_SEPARATE_LENGTH}
};

#define ACPI_FADT_INFO_ENTRIES        (sizeof (fadt_info_table) / sizeof (struct acpi_fadt_info))


static inline void
acpi_tb_init_generic_address(struct acpi_generic_address *generic_address,
			     u8 space_id, u8 byte_width, u64 address)
{

	/*
	 * The 64-bit Address field is non-aligned in the byte packed
	 * GAS struct.
	 */
	ACPI_MOVE_64_TO_64(&generic_address->address, &address);

	/* All other fields are byte-wide */

	generic_address->space_id = space_id;
	generic_address->bit_width = (u8)ACPI_MUL_8(byte_width);
	generic_address->bit_offset = 0;
	generic_address->access_width = 0;	/* Access width ANY */
}


void acpi_tb_parse_fadt(u32 table_index, u8 flags)
{
	u32 length;
	struct acpi_table_header *table;

	/*
	 * The FADT has multiple versions with different lengths,
	 * and it contains pointers to both the DSDT and FACS tables.
	 *
	 * Get a local copy of the FADT and convert it to a common format
	 * Map entire FADT, assumed to be smaller than one page.
	 */
	length = acpi_gbl_root_table_list.tables[table_index].length;

	table =
	    acpi_os_map_memory(acpi_gbl_root_table_list.tables[table_index].
			       address, length);
	if (!table) {
		return;
	}

	/*
	 * Validate the FADT checksum before we copy the table. Ignore
	 * checksum error as we want to try to get the DSDT and FACS.
	 */
	(void)acpi_tb_verify_checksum(table, length);

	/* Obtain a local copy of the FADT in common ACPI 2.0+ format */

	acpi_tb_create_local_fadt(table, length);

	/* All done with the real FADT, unmap it */

	acpi_os_unmap_memory(table, length);

	/* Obtain the DSDT and FACS tables via their addresses within the FADT */

	acpi_tb_install_table((acpi_physical_address) acpi_gbl_FADT.Xdsdt,
			      flags, ACPI_SIG_DSDT, ACPI_TABLE_INDEX_DSDT);

	acpi_tb_install_table((acpi_physical_address) acpi_gbl_FADT.Xfacs,
			      flags, ACPI_SIG_FACS, ACPI_TABLE_INDEX_FACS);
}


void acpi_tb_create_local_fadt(struct acpi_table_header *table, u32 length)
{

	/*
	 * Check if the FADT is larger than the largest table that we expect
	 * (the ACPI 2.0/3.0 version). If so, truncate the table, and issue
	 * a warning.
	 */
	if (length > sizeof(struct acpi_table_fadt)) {
		ACPI_WARNING((AE_INFO,
			      "FADT (revision %u) is longer than ACPI 2.0 version, "
			      "truncating length 0x%X to 0x%zX",
			      table->revision, (unsigned)length,
			      sizeof(struct acpi_table_fadt)));
	}

	/* Clear the entire local FADT */

	ACPI_MEMSET(&acpi_gbl_FADT, 0, sizeof(struct acpi_table_fadt));

	/* Copy the original FADT, up to sizeof (struct acpi_table_fadt) */

	ACPI_MEMCPY(&acpi_gbl_FADT, table,
		    ACPI_MIN(length, sizeof(struct acpi_table_fadt)));

	/*
	 * 1) Convert the local copy of the FADT to the common internal format
	 * 2) Validate some of the important values within the FADT
	 */
	acpi_tb_convert_fadt();
}


static void acpi_tb_convert_fadt(void)
{
	u8 pm1_register_bit_width;
	u8 pm1_register_byte_width;
	struct acpi_generic_address *target64;
	u32 i;

	/* Update the local FADT table header length */

	acpi_gbl_FADT.header.length = sizeof(struct acpi_table_fadt);

	/*
	 * Expand the 32-bit FACS and DSDT addresses to 64-bit as necessary.
	 * Later code will always use the X 64-bit field. Also, check for an
	 * address mismatch between the 32-bit and 64-bit address fields
	 * (FIRMWARE_CTRL/X_FIRMWARE_CTRL, DSDT/X_DSDT) which would indicate
	 * the presence of two FACS or two DSDT tables.
	 */
	if (!acpi_gbl_FADT.Xfacs) {
		acpi_gbl_FADT.Xfacs = (u64) acpi_gbl_FADT.facs;
	} else if (acpi_gbl_FADT.facs &&
		   (acpi_gbl_FADT.Xfacs != (u64) acpi_gbl_FADT.facs)) {
		ACPI_WARNING((AE_INFO,
		    "32/64 FACS address mismatch in FADT - two FACS tables!"));
	}

	if (!acpi_gbl_FADT.Xdsdt) {
		acpi_gbl_FADT.Xdsdt = (u64) acpi_gbl_FADT.dsdt;
	} else if (acpi_gbl_FADT.dsdt &&
		   (acpi_gbl_FADT.Xdsdt != (u64) acpi_gbl_FADT.dsdt)) {
		ACPI_WARNING((AE_INFO,
		    "32/64 DSDT address mismatch in FADT - two DSDT tables!"));
	}

	/*
	 * For ACPI 1.0 FADTs (revision 1 or 2), ensure that reserved fields which
	 * should be zero are indeed zero. This will workaround BIOSs that
	 * inadvertently place values in these fields.
	 *
	 * The ACPI 1.0 reserved fields that will be zeroed are the bytes located at
	 * offset 45, 55, 95, and the word located at offset 109, 110.
	 */
	if (acpi_gbl_FADT.header.revision < FADT2_REVISION_ID) {
		acpi_gbl_FADT.preferred_profile = 0;
		acpi_gbl_FADT.pstate_control = 0;
		acpi_gbl_FADT.cst_control = 0;
		acpi_gbl_FADT.boot_flags = 0;
	}

	/*
	 * Expand the ACPI 1.0 32-bit addresses to the ACPI 2.0 64-bit "X"
	 * generic address structures as necessary. Later code will always use
	 * the 64-bit address structures.
	 */
	for (i = 0; i < ACPI_FADT_INFO_ENTRIES; i++) {
		target64 =
		    ACPI_ADD_PTR(struct acpi_generic_address, &acpi_gbl_FADT,
				 fadt_info_table[i].address64);

		/* Expand only if the 64-bit X target is null */

		if (!target64->address) {

			/* The space_id is always I/O for the 32-bit legacy address fields */

			acpi_tb_init_generic_address(target64,
						     ACPI_ADR_SPACE_SYSTEM_IO,
						     *ACPI_ADD_PTR(u8,
								   &acpi_gbl_FADT,
								   fadt_info_table
								   [i].length),
						     (u64) * ACPI_ADD_PTR(u32,
									  &acpi_gbl_FADT,
									  fadt_info_table
									  [i].
									  address32));
		}
	}

	/* Validate FADT values now, before we make any changes */

	acpi_tb_validate_fadt();

	/*
	 * Optionally check all register lengths against the default values and
	 * update them if they are incorrect.
	 */
	if (acpi_gbl_use_default_register_widths) {
		for (i = 0; i < ACPI_FADT_INFO_ENTRIES; i++) {
			target64 =
			    ACPI_ADD_PTR(struct acpi_generic_address,
					 &acpi_gbl_FADT,
					 fadt_info_table[i].address64);

			/*
			 * If a valid register (Address != 0) and the (default_length > 0)
			 * (Not a GPE register), then check the width against the default.
			 */
			if ((target64->address) &&
			    (fadt_info_table[i].default_length > 0) &&
			    (fadt_info_table[i].default_length !=
			     target64->bit_width)) {
				ACPI_WARNING((AE_INFO,
					      "Invalid length for %s: %d, using default %d",
					      fadt_info_table[i].name,
					      target64->bit_width,
					      fadt_info_table[i].
					      default_length));

				/* Incorrect size, set width to the default */

				target64->bit_width =
				    fadt_info_table[i].default_length;
			}
		}
	}

	/*
	 * Get the length of the individual PM1 registers (enable and status).
	 * Each register is defined to be (event block length / 2).
	 */
	pm1_register_bit_width =
	    (u8)ACPI_DIV_2(acpi_gbl_FADT.xpm1a_event_block.bit_width);
	pm1_register_byte_width = (u8)ACPI_DIV_8(pm1_register_bit_width);

	/*
	 * Adjust the lengths of the PM1 Event Blocks so that they can be used to
	 * access the PM1 status register(s). Use (width / 2)
	 */
	acpi_gbl_FADT.xpm1a_event_block.bit_width = pm1_register_bit_width;
	acpi_gbl_FADT.xpm1b_event_block.bit_width = pm1_register_bit_width;

	/*
	 * Calculate separate GAS structs for the PM1 Enable registers.
	 * These addresses do not appear (directly) in the FADT, so it is
	 * useful to calculate them once, here.
	 *
	 * The PM event blocks are split into two register blocks, first is the
	 * PM Status Register block, followed immediately by the PM Enable
	 * Register block. Each is of length (xpm1x_event_block.bit_width/2).
	 *
	 * On various systems the v2 fields (and particularly the bit widths)
	 * cannot be relied upon, though. Hence resort to using the v1 length
	 * here (and warn about the inconsistency).
	 */
	if (acpi_gbl_FADT.xpm1a_event_block.bit_width
	    != acpi_gbl_FADT.pm1_event_length * 8)
		printk(KERN_WARNING "FADT: "
		       "X_PM1a_EVT_BLK.bit_width (%u) does not match"
		       " PM1_EVT_LEN (%u)\n",
		       acpi_gbl_FADT.xpm1a_event_block.bit_width,
		       acpi_gbl_FADT.pm1_event_length);

	/* The PM1A register block is required */

	acpi_tb_init_generic_address(&acpi_gbl_xpm1a_enable,
				     acpi_gbl_FADT.xpm1a_event_block.space_id,
				     pm1_register_byte_width,
				     (acpi_gbl_FADT.xpm1a_event_block.address +
				      pm1_register_byte_width));
	/* Don't forget to copy space_id of the GAS */
	acpi_gbl_xpm1a_enable.space_id =
	    acpi_gbl_FADT.xpm1a_event_block.space_id;

	/* The PM1B register block is optional, ignore if not present */

	if (acpi_gbl_FADT.xpm1b_event_block.address) {
		if (acpi_gbl_FADT.xpm1b_event_block.bit_width
		    != acpi_gbl_FADT.pm1_event_length * 8)
			printk(KERN_WARNING "FADT: "
			       "X_PM1b_EVT_BLK.bit_width (%u) does not match"
			       " PM1_EVT_LEN (%u)\n",
			       acpi_gbl_FADT.xpm1b_event_block.bit_width,
			       acpi_gbl_FADT.pm1_event_length);
		acpi_tb_init_generic_address(&acpi_gbl_xpm1b_enable,
					     acpi_gbl_FADT.xpm1b_event_block.space_id,
					     pm1_register_byte_width,
					     (acpi_gbl_FADT.xpm1b_event_block.
					      address + pm1_register_byte_width));
		/* Don't forget to copy space_id of the GAS */
		acpi_gbl_xpm1b_enable.space_id =
		    acpi_gbl_FADT.xpm1b_event_block.space_id;

	}
}


static void acpi_tb_validate_fadt(void)
{
	char *name;
	u32 *address32;
	struct acpi_generic_address *address64;
	u8 length;
	u32 i;

	/*
	 * Check for FACS and DSDT address mismatches. An address mismatch between
	 * the 32-bit and 64-bit address fields (FIRMWARE_CTRL/X_FIRMWARE_CTRL and
	 * DSDT/X_DSDT) would indicate the presence of two FACS or two DSDT tables.
	 */
	if (acpi_gbl_FADT.facs &&
	    (acpi_gbl_FADT.Xfacs != (u64) acpi_gbl_FADT.facs)) {
		ACPI_WARNING((AE_INFO,
			      "32/64X FACS address mismatch in FADT - "
			      "two FACS tables! %8.8X/%8.8X%8.8X",
			      acpi_gbl_FADT.facs,
			      ACPI_FORMAT_UINT64(acpi_gbl_FADT.Xfacs)));
	}

	if (acpi_gbl_FADT.dsdt &&
	    (acpi_gbl_FADT.Xdsdt != (u64) acpi_gbl_FADT.dsdt)) {
		ACPI_WARNING((AE_INFO,
			      "32/64X DSDT address mismatch in FADT - "
			      "two DSDT tables! %8.8X/%8.8X%8.8X",
			      acpi_gbl_FADT.dsdt,
			      ACPI_FORMAT_UINT64(acpi_gbl_FADT.Xdsdt)));
	}

	/* Examine all of the 64-bit extended address fields (X fields) */

	for (i = 0; i < ACPI_FADT_INFO_ENTRIES; i++) {
		/*
		 * Generate pointers to the 32-bit and 64-bit addresses, get the
		 * register length (width), and the register name
		 */
		address64 = ACPI_ADD_PTR(struct acpi_generic_address,
					 &acpi_gbl_FADT,
					 fadt_info_table[i].address64);
		address32 =
		    ACPI_ADD_PTR(u32, &acpi_gbl_FADT,
				 fadt_info_table[i].address32);
		length =
		    *ACPI_ADD_PTR(u8, &acpi_gbl_FADT,
				  fadt_info_table[i].length);
		name = fadt_info_table[i].name;

		/*
		 * For each extended field, check for length mismatch between the
		 * legacy length field and the corresponding 64-bit X length field.
		 */
		if (address64 && (address64->bit_width != ACPI_MUL_8(length))) {
			ACPI_WARNING((AE_INFO,
				      "32/64X length mismatch in %s: %d/%d",
				      name, ACPI_MUL_8(length),
				      address64->bit_width));
		}

		if (fadt_info_table[i].type & ACPI_FADT_REQUIRED) {
			/*
			 * Field is required (Pm1a_event, Pm1a_control, pm_timer).
			 * Both the address and length must be non-zero.
			 */
			if (!address64->address || !length) {
				ACPI_ERROR((AE_INFO,
					    "Required field %s has zero address and/or length: %8.8X%8.8X/%X",
					    name,
					    ACPI_FORMAT_UINT64(address64->
							       address),
					    length));
			}
		} else if (fadt_info_table[i].type & ACPI_FADT_SEPARATE_LENGTH) {
			/*
			 * Field is optional (PM2Control, GPE0, GPE1) AND has its own
			 * length field. If present, both the address and length must be valid.
			 */
			if ((address64->address && !length)
			    || (!address64->address && length)) {
				ACPI_WARNING((AE_INFO,
					      "Optional field %s has zero address or length: %8.8X%8.8X/%X",
					      name,
					      ACPI_FORMAT_UINT64(address64->
								 address),
					      length));
			}
		}

		/* If both 32- and 64-bit addresses are valid (non-zero), they must match */

		if (address64->address && *address32 &&
		    (address64->address != (u64) * address32)) {
			ACPI_ERROR((AE_INFO,
				    "32/64X address mismatch in %s: %8.8X/%8.8X%8.8X, using 64X",
				    name, *address32,
				    ACPI_FORMAT_UINT64(address64->address)));
		}
	}
}
