


#include <acpi/acpi.h>
#include "accommon.h"
#include "acdispat.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_EXECUTER
ACPI_MODULE_NAME("exfield")

acpi_status
acpi_ex_read_data_from_field(struct acpi_walk_state *walk_state,
			     union acpi_operand_object *obj_desc,
			     union acpi_operand_object **ret_buffer_desc)
{
	acpi_status status;
	union acpi_operand_object *buffer_desc;
	acpi_size length;
	void *buffer;

	ACPI_FUNCTION_TRACE_PTR(ex_read_data_from_field, obj_desc);

	/* Parameter validation */

	if (!obj_desc) {
		return_ACPI_STATUS(AE_AML_NO_OPERAND);
	}
	if (!ret_buffer_desc) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	if (ACPI_GET_OBJECT_TYPE(obj_desc) == ACPI_TYPE_BUFFER_FIELD) {
		/*
		 * If the buffer_field arguments have not been previously evaluated,
		 * evaluate them now and save the results.
		 */
		if (!(obj_desc->common.flags & AOPOBJ_DATA_VALID)) {
			status = acpi_ds_get_buffer_field_arguments(obj_desc);
			if (ACPI_FAILURE(status)) {
				return_ACPI_STATUS(status);
			}
		}
	} else
	    if ((ACPI_GET_OBJECT_TYPE(obj_desc) == ACPI_TYPE_LOCAL_REGION_FIELD)
		&& (obj_desc->field.region_obj->region.space_id ==
		    ACPI_ADR_SPACE_SMBUS)) {
		/*
		 * This is an SMBus read.  We must create a buffer to hold the data
		 * and directly access the region handler.
		 */
		buffer_desc =
		    acpi_ut_create_buffer_object(ACPI_SMBUS_BUFFER_SIZE);
		if (!buffer_desc) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		/* Lock entire transaction if requested */

		acpi_ex_acquire_global_lock(obj_desc->common_field.field_flags);

		/*
		 * Perform the read.
		 * Note: Smbus protocol value is passed in upper 16-bits of Function
		 */
		status = acpi_ex_access_region(obj_desc, 0,
					       ACPI_CAST_PTR(acpi_integer,
							     buffer_desc->
							     buffer.pointer),
					       ACPI_READ | (obj_desc->field.
							    attribute << 16));
		acpi_ex_release_global_lock(obj_desc->common_field.field_flags);
		goto exit;
	}

	/*
	 * Allocate a buffer for the contents of the field.
	 *
	 * If the field is larger than the size of an acpi_integer, create
	 * a BUFFER to hold it.  Otherwise, use an INTEGER.  This allows
	 * the use of arithmetic operators on the returned value if the
	 * field size is equal or smaller than an Integer.
	 *
	 * Note: Field.length is in bits.
	 */
	length =
	    (acpi_size) ACPI_ROUND_BITS_UP_TO_BYTES(obj_desc->field.bit_length);
	if (length > acpi_gbl_integer_byte_width) {

		/* Field is too large for an Integer, create a Buffer instead */

		buffer_desc = acpi_ut_create_buffer_object(length);
		if (!buffer_desc) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}
		buffer = buffer_desc->buffer.pointer;
	} else {
		/* Field will fit within an Integer (normal case) */

		buffer_desc = acpi_ut_create_internal_object(ACPI_TYPE_INTEGER);
		if (!buffer_desc) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		length = acpi_gbl_integer_byte_width;
		buffer_desc->integer.value = 0;
		buffer = &buffer_desc->integer.value;
	}

	ACPI_DEBUG_PRINT((ACPI_DB_BFIELD,
			  "FieldRead [TO]:   Obj %p, Type %X, Buf %p, ByteLen %X\n",
			  obj_desc, ACPI_GET_OBJECT_TYPE(obj_desc), buffer,
			  (u32) length));
	ACPI_DEBUG_PRINT((ACPI_DB_BFIELD,
			  "FieldRead [FROM]: BitLen %X, BitOff %X, ByteOff %X\n",
			  obj_desc->common_field.bit_length,
			  obj_desc->common_field.start_field_bit_offset,
			  obj_desc->common_field.base_byte_offset));

	/* Lock entire transaction if requested */

	acpi_ex_acquire_global_lock(obj_desc->common_field.field_flags);

	/* Read from the field */

	status = acpi_ex_extract_from_field(obj_desc, buffer, (u32) length);
	acpi_ex_release_global_lock(obj_desc->common_field.field_flags);

      exit:
	if (ACPI_FAILURE(status)) {
		acpi_ut_remove_reference(buffer_desc);
	} else {
		*ret_buffer_desc = buffer_desc;
	}

	return_ACPI_STATUS(status);
}


acpi_status
acpi_ex_write_data_to_field(union acpi_operand_object *source_desc,
			    union acpi_operand_object *obj_desc,
			    union acpi_operand_object **result_desc)
{
	acpi_status status;
	u32 length;
	void *buffer;
	union acpi_operand_object *buffer_desc;

	ACPI_FUNCTION_TRACE_PTR(ex_write_data_to_field, obj_desc);

	/* Parameter validation */

	if (!source_desc || !obj_desc) {
		return_ACPI_STATUS(AE_AML_NO_OPERAND);
	}

	if (ACPI_GET_OBJECT_TYPE(obj_desc) == ACPI_TYPE_BUFFER_FIELD) {
		/*
		 * If the buffer_field arguments have not been previously evaluated,
		 * evaluate them now and save the results.
		 */
		if (!(obj_desc->common.flags & AOPOBJ_DATA_VALID)) {
			status = acpi_ds_get_buffer_field_arguments(obj_desc);
			if (ACPI_FAILURE(status)) {
				return_ACPI_STATUS(status);
			}
		}
	} else
	    if ((ACPI_GET_OBJECT_TYPE(obj_desc) == ACPI_TYPE_LOCAL_REGION_FIELD)
		&& (obj_desc->field.region_obj->region.space_id ==
		    ACPI_ADR_SPACE_SMBUS)) {
		/*
		 * This is an SMBus write.  We will bypass the entire field mechanism
		 * and handoff the buffer directly to the handler.
		 *
		 * Source must be a buffer of sufficient size (ACPI_SMBUS_BUFFER_SIZE).
		 */
		if (ACPI_GET_OBJECT_TYPE(source_desc) != ACPI_TYPE_BUFFER) {
			ACPI_ERROR((AE_INFO,
				    "SMBus write requires Buffer, found type %s",
				    acpi_ut_get_object_type_name(source_desc)));

			return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
		}

		if (source_desc->buffer.length < ACPI_SMBUS_BUFFER_SIZE) {
			ACPI_ERROR((AE_INFO,
				    "SMBus write requires Buffer of length %X, found length %X",
				    ACPI_SMBUS_BUFFER_SIZE,
				    source_desc->buffer.length));

			return_ACPI_STATUS(AE_AML_BUFFER_LIMIT);
		}

		buffer_desc =
		    acpi_ut_create_buffer_object(ACPI_SMBUS_BUFFER_SIZE);
		if (!buffer_desc) {
			return_ACPI_STATUS(AE_NO_MEMORY);
		}

		buffer = buffer_desc->buffer.pointer;
		ACPI_MEMCPY(buffer, source_desc->buffer.pointer,
			    ACPI_SMBUS_BUFFER_SIZE);

		/* Lock entire transaction if requested */

		acpi_ex_acquire_global_lock(obj_desc->common_field.field_flags);

		/*
		 * Perform the write (returns status and perhaps data in the
		 * same buffer)
		 * Note: SMBus protocol type is passed in upper 16-bits of Function.
		 */
		status = acpi_ex_access_region(obj_desc, 0,
					       (acpi_integer *) buffer,
					       ACPI_WRITE | (obj_desc->field.
							     attribute << 16));
		acpi_ex_release_global_lock(obj_desc->common_field.field_flags);

		*result_desc = buffer_desc;
		return_ACPI_STATUS(status);
	}

	/* Get a pointer to the data to be written */

	switch (ACPI_GET_OBJECT_TYPE(source_desc)) {
	case ACPI_TYPE_INTEGER:
		buffer = &source_desc->integer.value;
		length = sizeof(source_desc->integer.value);
		break;

	case ACPI_TYPE_BUFFER:
		buffer = source_desc->buffer.pointer;
		length = source_desc->buffer.length;
		break;

	case ACPI_TYPE_STRING:
		buffer = source_desc->string.pointer;
		length = source_desc->string.length;
		break;

	default:
		return_ACPI_STATUS(AE_AML_OPERAND_TYPE);
	}

	ACPI_DEBUG_PRINT((ACPI_DB_BFIELD,
			  "FieldWrite [FROM]: Obj %p (%s:%X), Buf %p, ByteLen %X\n",
			  source_desc,
			  acpi_ut_get_type_name(ACPI_GET_OBJECT_TYPE
						(source_desc)),
			  ACPI_GET_OBJECT_TYPE(source_desc), buffer, length));

	ACPI_DEBUG_PRINT((ACPI_DB_BFIELD,
			  "FieldWrite [TO]:   Obj %p (%s:%X), BitLen %X, BitOff %X, ByteOff %X\n",
			  obj_desc,
			  acpi_ut_get_type_name(ACPI_GET_OBJECT_TYPE(obj_desc)),
			  ACPI_GET_OBJECT_TYPE(obj_desc),
			  obj_desc->common_field.bit_length,
			  obj_desc->common_field.start_field_bit_offset,
			  obj_desc->common_field.base_byte_offset));

	/* Lock entire transaction if requested */

	acpi_ex_acquire_global_lock(obj_desc->common_field.field_flags);

	/* Write to the field */

	status = acpi_ex_insert_into_field(obj_desc, buffer, length);
	acpi_ex_release_global_lock(obj_desc->common_field.field_flags);

	return_ACPI_STATUS(status);
}
