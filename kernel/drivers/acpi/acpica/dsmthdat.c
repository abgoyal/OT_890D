


#include <acpi/acpi.h>
#include "accommon.h"
#include "acdispat.h"
#include "acnamesp.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_DISPATCHER
ACPI_MODULE_NAME("dsmthdat")

/* Local prototypes */
static void
acpi_ds_method_data_delete_value(u8 type,
				 u32 index, struct acpi_walk_state *walk_state);

static acpi_status
acpi_ds_method_data_set_value(u8 type,
			      u32 index,
			      union acpi_operand_object *object,
			      struct acpi_walk_state *walk_state);

#ifdef ACPI_OBSOLETE_FUNCTIONS
acpi_object_type
acpi_ds_method_data_get_type(u16 opcode,
			     u32 index, struct acpi_walk_state *walk_state);
#endif


void acpi_ds_method_data_init(struct acpi_walk_state *walk_state)
{
	u32 i;

	ACPI_FUNCTION_TRACE(ds_method_data_init);

	/* Init the method arguments */

	for (i = 0; i < ACPI_METHOD_NUM_ARGS; i++) {
		ACPI_MOVE_32_TO_32(&walk_state->arguments[i].name,
				   NAMEOF_ARG_NTE);
		walk_state->arguments[i].name.integer |= (i << 24);
		walk_state->arguments[i].descriptor_type = ACPI_DESC_TYPE_NAMED;
		walk_state->arguments[i].type = ACPI_TYPE_ANY;
		walk_state->arguments[i].flags =
		    ANOBJ_END_OF_PEER_LIST | ANOBJ_METHOD_ARG;
	}

	/* Init the method locals */

	for (i = 0; i < ACPI_METHOD_NUM_LOCALS; i++) {
		ACPI_MOVE_32_TO_32(&walk_state->local_variables[i].name,
				   NAMEOF_LOCAL_NTE);

		walk_state->local_variables[i].name.integer |= (i << 24);
		walk_state->local_variables[i].descriptor_type =
		    ACPI_DESC_TYPE_NAMED;
		walk_state->local_variables[i].type = ACPI_TYPE_ANY;
		walk_state->local_variables[i].flags =
		    ANOBJ_END_OF_PEER_LIST | ANOBJ_METHOD_LOCAL;
	}

	return_VOID;
}


void acpi_ds_method_data_delete_all(struct acpi_walk_state *walk_state)
{
	u32 index;

	ACPI_FUNCTION_TRACE(ds_method_data_delete_all);

	/* Detach the locals */

	for (index = 0; index < ACPI_METHOD_NUM_LOCALS; index++) {
		if (walk_state->local_variables[index].object) {
			ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "Deleting Local%d=%p\n",
					  index,
					  walk_state->local_variables[index].
					  object));

			/* Detach object (if present) and remove a reference */

			acpi_ns_detach_object(&walk_state->
					      local_variables[index]);
		}
	}

	/* Detach the arguments */

	for (index = 0; index < ACPI_METHOD_NUM_ARGS; index++) {
		if (walk_state->arguments[index].object) {
			ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "Deleting Arg%d=%p\n",
					  index,
					  walk_state->arguments[index].object));

			/* Detach object (if present) and remove a reference */

			acpi_ns_detach_object(&walk_state->arguments[index]);
		}
	}

	return_VOID;
}


acpi_status
acpi_ds_method_data_init_args(union acpi_operand_object **params,
			      u32 max_param_count,
			      struct acpi_walk_state *walk_state)
{
	acpi_status status;
	u32 index = 0;

	ACPI_FUNCTION_TRACE_PTR(ds_method_data_init_args, params);

	if (!params) {
		ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
				  "No param list passed to method\n"));
		return_ACPI_STATUS(AE_OK);
	}

	/* Copy passed parameters into the new method stack frame */

	while ((index < ACPI_METHOD_NUM_ARGS) &&
	       (index < max_param_count) && params[index]) {
		/*
		 * A valid parameter.
		 * Store the argument in the method/walk descriptor.
		 * Do not copy the arg in order to implement call by reference
		 */
		status = acpi_ds_method_data_set_value(ACPI_REFCLASS_ARG, index,
						       params[index],
						       walk_state);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}

		index++;
	}

	ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "%d args passed to method\n", index));
	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_ds_method_data_get_node(u8 type,
			     u32 index,
			     struct acpi_walk_state *walk_state,
			     struct acpi_namespace_node **node)
{
	ACPI_FUNCTION_TRACE(ds_method_data_get_node);

	/*
	 * Method Locals and Arguments are supported
	 */
	switch (type) {
	case ACPI_REFCLASS_LOCAL:

		if (index > ACPI_METHOD_MAX_LOCAL) {
			ACPI_ERROR((AE_INFO,
				    "Local index %d is invalid (max %d)",
				    index, ACPI_METHOD_MAX_LOCAL));
			return_ACPI_STATUS(AE_AML_INVALID_INDEX);
		}

		/* Return a pointer to the pseudo-node */

		*node = &walk_state->local_variables[index];
		break;

	case ACPI_REFCLASS_ARG:

		if (index > ACPI_METHOD_MAX_ARG) {
			ACPI_ERROR((AE_INFO,
				    "Arg index %d is invalid (max %d)",
				    index, ACPI_METHOD_MAX_ARG));
			return_ACPI_STATUS(AE_AML_INVALID_INDEX);
		}

		/* Return a pointer to the pseudo-node */

		*node = &walk_state->arguments[index];
		break;

	default:
		ACPI_ERROR((AE_INFO, "Type %d is invalid", type));
		return_ACPI_STATUS(AE_TYPE);
	}

	return_ACPI_STATUS(AE_OK);
}


static acpi_status
acpi_ds_method_data_set_value(u8 type,
			      u32 index,
			      union acpi_operand_object *object,
			      struct acpi_walk_state *walk_state)
{
	acpi_status status;
	struct acpi_namespace_node *node;

	ACPI_FUNCTION_TRACE(ds_method_data_set_value);

	ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
			  "NewObj %p Type %2.2X, Refs=%d [%s]\n", object,
			  type, object->common.reference_count,
			  acpi_ut_get_type_name(object->common.type)));

	/* Get the namespace node for the arg/local */

	status = acpi_ds_method_data_get_node(type, index, walk_state, &node);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	/*
	 * Increment ref count so object can't be deleted while installed.
	 * NOTE: We do not copy the object in order to preserve the call by
	 * reference semantics of ACPI Control Method invocation.
	 * (See ACPI Specification 2.0_c)
	 */
	acpi_ut_add_reference(object);

	/* Install the object */

	node->object = object;
	return_ACPI_STATUS(status);
}


acpi_status
acpi_ds_method_data_get_value(u8 type,
			      u32 index,
			      struct acpi_walk_state *walk_state,
			      union acpi_operand_object **dest_desc)
{
	acpi_status status;
	struct acpi_namespace_node *node;
	union acpi_operand_object *object;

	ACPI_FUNCTION_TRACE(ds_method_data_get_value);

	/* Validate the object descriptor */

	if (!dest_desc) {
		ACPI_ERROR((AE_INFO, "Null object descriptor pointer"));
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	/* Get the namespace node for the arg/local */

	status = acpi_ds_method_data_get_node(type, index, walk_state, &node);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	/* Get the object from the node */

	object = node->object;

	/* Examine the returned object, it must be valid. */

	if (!object) {
		/*
		 * Index points to uninitialized object.
		 * This means that either 1) The expected argument was
		 * not passed to the method, or 2) A local variable
		 * was referenced by the method (via the ASL)
		 * before it was initialized.  Either case is an error.
		 */

		/* If slack enabled, init the local_x/arg_x to an Integer of value zero */

		if (acpi_gbl_enable_interpreter_slack) {
			object =
			    acpi_ut_create_internal_object(ACPI_TYPE_INTEGER);
			if (!object) {
				return_ACPI_STATUS(AE_NO_MEMORY);
			}

			object->integer.value = 0;
			node->object = object;
		}

		/* Otherwise, return the error */

		else
			switch (type) {
			case ACPI_REFCLASS_ARG:

				ACPI_ERROR((AE_INFO,
					    "Uninitialized Arg[%d] at node %p",
					    index, node));

				return_ACPI_STATUS(AE_AML_UNINITIALIZED_ARG);

			case ACPI_REFCLASS_LOCAL:

				ACPI_ERROR((AE_INFO,
					    "Uninitialized Local[%d] at node %p",
					    index, node));

				return_ACPI_STATUS(AE_AML_UNINITIALIZED_LOCAL);

			default:

				ACPI_ERROR((AE_INFO,
					    "Not a Arg/Local opcode: %X",
					    type));
				return_ACPI_STATUS(AE_AML_INTERNAL);
			}
	}

	/*
	 * The Index points to an initialized and valid object.
	 * Return an additional reference to the object
	 */
	*dest_desc = object;
	acpi_ut_add_reference(object);

	return_ACPI_STATUS(AE_OK);
}


static void
acpi_ds_method_data_delete_value(u8 type,
				 u32 index, struct acpi_walk_state *walk_state)
{
	acpi_status status;
	struct acpi_namespace_node *node;
	union acpi_operand_object *object;

	ACPI_FUNCTION_TRACE(ds_method_data_delete_value);

	/* Get the namespace node for the arg/local */

	status = acpi_ds_method_data_get_node(type, index, walk_state, &node);
	if (ACPI_FAILURE(status)) {
		return_VOID;
	}

	/* Get the associated object */

	object = acpi_ns_get_attached_object(node);

	/*
	 * Undefine the Arg or Local by setting its descriptor
	 * pointer to NULL. Locals/Args can contain both
	 * ACPI_OPERAND_OBJECTS and ACPI_NAMESPACE_NODEs
	 */
	node->object = NULL;

	if ((object) &&
	    (ACPI_GET_DESCRIPTOR_TYPE(object) == ACPI_DESC_TYPE_OPERAND)) {
		/*
		 * There is a valid object.
		 * Decrement the reference count by one to balance the
		 * increment when the object was stored.
		 */
		acpi_ut_remove_reference(object);
	}

	return_VOID;
}


acpi_status
acpi_ds_store_object_to_local(u8 type,
			      u32 index,
			      union acpi_operand_object *obj_desc,
			      struct acpi_walk_state *walk_state)
{
	acpi_status status;
	struct acpi_namespace_node *node;
	union acpi_operand_object *current_obj_desc;
	union acpi_operand_object *new_obj_desc;

	ACPI_FUNCTION_TRACE(ds_store_object_to_local);
	ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "Type=%2.2X Index=%d Obj=%p\n",
			  type, index, obj_desc));

	/* Parameter validation */

	if (!obj_desc) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	/* Get the namespace node for the arg/local */

	status = acpi_ds_method_data_get_node(type, index, walk_state, &node);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	current_obj_desc = acpi_ns_get_attached_object(node);
	if (current_obj_desc == obj_desc) {
		ACPI_DEBUG_PRINT((ACPI_DB_EXEC, "Obj=%p already installed!\n",
				  obj_desc));
		return_ACPI_STATUS(status);
	}

	/*
	 * If the reference count on the object is more than one, we must
	 * take a copy of the object before we store.  A reference count
	 * of exactly 1 means that the object was just created during the
	 * evaluation of an expression, and we can safely use it since it
	 * is not used anywhere else.
	 */
	new_obj_desc = obj_desc;
	if (obj_desc->common.reference_count > 1) {
		status =
		    acpi_ut_copy_iobject_to_iobject(obj_desc, &new_obj_desc,
						    walk_state);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}
	}

	/*
	 * If there is an object already in this slot, we either
	 * have to delete it, or if this is an argument and there
	 * is an object reference stored there, we have to do
	 * an indirect store!
	 */
	if (current_obj_desc) {
		/*
		 * Check for an indirect store if an argument
		 * contains an object reference (stored as an Node).
		 * We don't allow this automatic dereferencing for
		 * locals, since a store to a local should overwrite
		 * anything there, including an object reference.
		 *
		 * If both Arg0 and Local0 contain ref_of (Local4):
		 *
		 * Store (1, Arg0)             - Causes indirect store to local4
		 * Store (1, Local0)           - Stores 1 in local0, overwriting
		 *                                  the reference to local4
		 * Store (1, de_refof (Local0)) - Causes indirect store to local4
		 *
		 * Weird, but true.
		 */
		if (type == ACPI_REFCLASS_ARG) {
			/*
			 * If we have a valid reference object that came from ref_of(),
			 * do the indirect store
			 */
			if ((ACPI_GET_DESCRIPTOR_TYPE(current_obj_desc) ==
			     ACPI_DESC_TYPE_OPERAND)
			    && (current_obj_desc->common.type ==
				ACPI_TYPE_LOCAL_REFERENCE)
			    && (current_obj_desc->reference.class ==
				ACPI_REFCLASS_REFOF)) {
				ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
						  "Arg (%p) is an ObjRef(Node), storing in node %p\n",
						  new_obj_desc,
						  current_obj_desc));

				/*
				 * Store this object to the Node (perform the indirect store)
				 * NOTE: No implicit conversion is performed, as per the ACPI
				 * specification rules on storing to Locals/Args.
				 */
				status =
				    acpi_ex_store_object_to_node(new_obj_desc,
								 current_obj_desc->
								 reference.
								 object,
								 walk_state,
								 ACPI_NO_IMPLICIT_CONVERSION);

				/* Remove local reference if we copied the object above */

				if (new_obj_desc != obj_desc) {
					acpi_ut_remove_reference(new_obj_desc);
				}
				return_ACPI_STATUS(status);
			}
		}

		/* Delete the existing object before storing the new one */

		acpi_ds_method_data_delete_value(type, index, walk_state);
	}

	/*
	 * Install the Obj descriptor (*new_obj_desc) into
	 * the descriptor for the Arg or Local.
	 * (increments the object reference count by one)
	 */
	status =
	    acpi_ds_method_data_set_value(type, index, new_obj_desc,
					  walk_state);

	/* Remove local reference if we copied the object above */

	if (new_obj_desc != obj_desc) {
		acpi_ut_remove_reference(new_obj_desc);
	}

	return_ACPI_STATUS(status);
}

#ifdef ACPI_OBSOLETE_FUNCTIONS

acpi_object_type
acpi_ds_method_data_get_type(u16 opcode,
			     u32 index, struct acpi_walk_state *walk_state)
{
	acpi_status status;
	struct acpi_namespace_node *node;
	union acpi_operand_object *object;

	ACPI_FUNCTION_TRACE(ds_method_data_get_type);

	/* Get the namespace node for the arg/local */

	status = acpi_ds_method_data_get_node(opcode, index, walk_state, &node);
	if (ACPI_FAILURE(status)) {
		return_VALUE((ACPI_TYPE_NOT_FOUND));
	}

	/* Get the object */

	object = acpi_ns_get_attached_object(node);
	if (!object) {

		/* Uninitialized local/arg, return TYPE_ANY */

		return_VALUE(ACPI_TYPE_ANY);
	}

	/* Get the object type */

	return_VALUE(ACPI_GET_OBJECT_TYPE(object));
}
#endif
