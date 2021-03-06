



#include <acpi/acpi.h>
#include "accommon.h"
#include "acinterp.h"
#include "amlcode.h"

#define _COMPONENT          ACPI_EXECUTER
ACPI_MODULE_NAME("exstoren")

acpi_status
acpi_ex_resolve_object(union acpi_operand_object **source_desc_ptr,
		       acpi_object_type target_type,
		       struct acpi_walk_state *walk_state)
{
	union acpi_operand_object *source_desc = *source_desc_ptr;
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ex_resolve_object);

	/* Ensure we have a Target that can be stored to */

	switch (target_type) {
	case ACPI_TYPE_BUFFER_FIELD:
	case ACPI_TYPE_LOCAL_REGION_FIELD:
	case ACPI_TYPE_LOCAL_BANK_FIELD:
	case ACPI_TYPE_LOCAL_INDEX_FIELD:
		/*
		 * These cases all require only Integers or values that
		 * can be converted to Integers (Strings or Buffers)
		 */

	case ACPI_TYPE_INTEGER:
	case ACPI_TYPE_STRING:
	case ACPI_TYPE_BUFFER:

		/*
		 * Stores into a Field/Region or into a Integer/Buffer/String
		 * are all essentially the same.  This case handles the
		 * "interchangeable" types Integer, String, and Buffer.
		 */
		if (ACPI_GET_OBJECT_TYPE(source_desc) ==
		    ACPI_TYPE_LOCAL_REFERENCE) {

			/* Resolve a reference object first */

			status =
			    acpi_ex_resolve_to_value(source_desc_ptr,
						     walk_state);
			if (ACPI_FAILURE(status)) {
				break;
			}
		}

		/* For copy_object, no further validation necessary */

		if (walk_state->opcode == AML_COPY_OP) {
			break;
		}

		/* Must have a Integer, Buffer, or String */

		if ((ACPI_GET_OBJECT_TYPE(source_desc) != ACPI_TYPE_INTEGER) &&
		    (ACPI_GET_OBJECT_TYPE(source_desc) != ACPI_TYPE_BUFFER) &&
		    (ACPI_GET_OBJECT_TYPE(source_desc) != ACPI_TYPE_STRING) &&
		    !((ACPI_GET_OBJECT_TYPE(source_desc) ==
		       ACPI_TYPE_LOCAL_REFERENCE)
		      && (source_desc->reference.class ==
			  ACPI_REFCLASS_TABLE))) {

			/* Conversion successful but still not a valid type */

			ACPI_ERROR((AE_INFO,
				    "Cannot assign type %s to %s (must be type Int/Str/Buf)",
				    acpi_ut_get_object_type_name(source_desc),
				    acpi_ut_get_type_name(target_type)));
			status = AE_AML_OPERAND_TYPE;
		}
		break;

	case ACPI_TYPE_LOCAL_ALIAS:
	case ACPI_TYPE_LOCAL_METHOD_ALIAS:

		/*
		 * All aliases should have been resolved earlier, during the
		 * operand resolution phase.
		 */
		ACPI_ERROR((AE_INFO, "Store into an unresolved Alias object"));
		status = AE_AML_INTERNAL;
		break;

	case ACPI_TYPE_PACKAGE:
	default:

		/*
		 * All other types than Alias and the various Fields come here,
		 * including the untyped case - ACPI_TYPE_ANY.
		 */
		break;
	}

	return_ACPI_STATUS(status);
}


acpi_status
acpi_ex_store_object_to_object(union acpi_operand_object *source_desc,
			       union acpi_operand_object *dest_desc,
			       union acpi_operand_object **new_desc,
			       struct acpi_walk_state *walk_state)
{
	union acpi_operand_object *actual_src_desc;
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE_PTR(ex_store_object_to_object, source_desc);

	actual_src_desc = source_desc;
	if (!dest_desc) {
		/*
		 * There is no destination object (An uninitialized node or
		 * package element), so we can simply copy the source object
		 * creating a new destination object
		 */
		status =
		    acpi_ut_copy_iobject_to_iobject(actual_src_desc, new_desc,
						    walk_state);
		return_ACPI_STATUS(status);
	}

	if (ACPI_GET_OBJECT_TYPE(source_desc) !=
	    ACPI_GET_OBJECT_TYPE(dest_desc)) {
		/*
		 * The source type does not match the type of the destination.
		 * Perform the "implicit conversion" of the source to the current type
		 * of the target as per the ACPI specification.
		 *
		 * If no conversion performed, actual_src_desc = source_desc.
		 * Otherwise, actual_src_desc is a temporary object to hold the
		 * converted object.
		 */
		status =
		    acpi_ex_convert_to_target_type(ACPI_GET_OBJECT_TYPE
						   (dest_desc), source_desc,
						   &actual_src_desc,
						   walk_state);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

		if (source_desc == actual_src_desc) {
			/*
			 * No conversion was performed. Return the source_desc as the
			 * new object.
			 */
			*new_desc = source_desc;
			return_ACPI_STATUS(AE_OK);
		}
	}

	/*
	 * We now have two objects of identical types, and we can perform a
	 * copy of the *value* of the source object.
	 */
	switch (ACPI_GET_OBJECT_TYPE(dest_desc)) {
	case ACPI_TYPE_INTEGER:

		dest_desc->integer.value = actual_src_desc->integer.value;

		/* Truncate value if we are executing from a 32-bit ACPI table */

		acpi_ex_truncate_for32bit_table(dest_desc);
		break;

	case ACPI_TYPE_STRING:

		status =
		    acpi_ex_store_string_to_string(actual_src_desc, dest_desc);
		break;

	case ACPI_TYPE_BUFFER:

		status =
		    acpi_ex_store_buffer_to_buffer(actual_src_desc, dest_desc);
		break;

	case ACPI_TYPE_PACKAGE:

		status =
		    acpi_ut_copy_iobject_to_iobject(actual_src_desc, &dest_desc,
						    walk_state);
		break;

	default:
		/*
		 * All other types come here.
		 */
		ACPI_WARNING((AE_INFO, "Store into type %s not implemented",
			      acpi_ut_get_object_type_name(dest_desc)));

		status = AE_NOT_IMPLEMENTED;
		break;
	}

	if (actual_src_desc != source_desc) {

		/* Delete the intermediate (temporary) source object */

		acpi_ut_remove_reference(actual_src_desc);
	}

	*new_desc = dest_desc;
	return_ACPI_STATUS(status);
}
