



#include <acpi/acpi.h>
#include "accommon.h"
#include "acinterp.h"
#include "acevents.h"

#define _COMPONENT          ACPI_EXECUTER
ACPI_MODULE_NAME("exmutex")

/* Local prototypes */
static void
acpi_ex_link_mutex(union acpi_operand_object *obj_desc,
		   struct acpi_thread_state *thread);


void acpi_ex_unlink_mutex(union acpi_operand_object *obj_desc)
{
	struct acpi_thread_state *thread = obj_desc->mutex.owner_thread;

	if (!thread) {
		return;
	}

	/* Doubly linked list */

	if (obj_desc->mutex.next) {
		(obj_desc->mutex.next)->mutex.prev = obj_desc->mutex.prev;
	}

	if (obj_desc->mutex.prev) {
		(obj_desc->mutex.prev)->mutex.next = obj_desc->mutex.next;
	} else {
		thread->acquired_mutex_list = obj_desc->mutex.next;
	}
}


static void
acpi_ex_link_mutex(union acpi_operand_object *obj_desc,
		   struct acpi_thread_state *thread)
{
	union acpi_operand_object *list_head;

	list_head = thread->acquired_mutex_list;

	/* This object will be the first object in the list */

	obj_desc->mutex.prev = NULL;
	obj_desc->mutex.next = list_head;

	/* Update old first object to point back to this object */

	if (list_head) {
		list_head->mutex.prev = obj_desc;
	}

	/* Update list head */

	thread->acquired_mutex_list = obj_desc;
}


acpi_status
acpi_ex_acquire_mutex_object(u16 timeout,
			     union acpi_operand_object *obj_desc,
			     acpi_thread_id thread_id)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE_PTR(ex_acquire_mutex_object, obj_desc);

	if (!obj_desc) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	/* Support for multiple acquires by the owning thread */

	if (obj_desc->mutex.thread_id == thread_id) {
		/*
		 * The mutex is already owned by this thread, just increment the
		 * acquisition depth
		 */
		obj_desc->mutex.acquisition_depth++;
		return_ACPI_STATUS(AE_OK);
	}

	/* Acquire the mutex, wait if necessary. Special case for Global Lock */

	if (obj_desc == acpi_gbl_global_lock_mutex) {
		status = acpi_ev_acquire_global_lock(timeout);
	} else {
		status = acpi_ex_system_wait_mutex(obj_desc->mutex.os_mutex,
						   timeout);
	}

	if (ACPI_FAILURE(status)) {

		/* Includes failure from a timeout on time_desc */

		return_ACPI_STATUS(status);
	}

	/* Acquired the mutex: update mutex object */

	obj_desc->mutex.thread_id = thread_id;
	obj_desc->mutex.acquisition_depth = 1;
	obj_desc->mutex.original_sync_level = 0;
	obj_desc->mutex.owner_thread = NULL;	/* Used only for AML Acquire() */

	return_ACPI_STATUS(AE_OK);
}


acpi_status
acpi_ex_acquire_mutex(union acpi_operand_object *time_desc,
		      union acpi_operand_object *obj_desc,
		      struct acpi_walk_state *walk_state)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE_PTR(ex_acquire_mutex, obj_desc);

	if (!obj_desc) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	/* Must have a valid thread ID */

	if (!walk_state->thread) {
		ACPI_ERROR((AE_INFO,
			    "Cannot acquire Mutex [%4.4s], null thread info",
			    acpi_ut_get_node_name(obj_desc->mutex.node)));
		return_ACPI_STATUS(AE_AML_INTERNAL);
	}

	/*
	 * Current sync level must be less than or equal to the sync level of the
	 * mutex. This mechanism provides some deadlock prevention
	 */
	if (walk_state->thread->current_sync_level > obj_desc->mutex.sync_level) {
		ACPI_ERROR((AE_INFO,
			    "Cannot acquire Mutex [%4.4s], current SyncLevel is too large (%d)",
			    acpi_ut_get_node_name(obj_desc->mutex.node),
			    walk_state->thread->current_sync_level));
		return_ACPI_STATUS(AE_AML_MUTEX_ORDER);
	}

	status = acpi_ex_acquire_mutex_object((u16) time_desc->integer.value,
					      obj_desc,
					      walk_state->thread->thread_id);
	if (ACPI_SUCCESS(status) && obj_desc->mutex.acquisition_depth == 1) {

		/* Save Thread object, original/current sync levels */

		obj_desc->mutex.owner_thread = walk_state->thread;
		obj_desc->mutex.original_sync_level =
		    walk_state->thread->current_sync_level;
		walk_state->thread->current_sync_level =
		    obj_desc->mutex.sync_level;

		/* Link the mutex to the current thread for force-unlock at method exit */

		acpi_ex_link_mutex(obj_desc, walk_state->thread);
	}

	return_ACPI_STATUS(status);
}


acpi_status acpi_ex_release_mutex_object(union acpi_operand_object *obj_desc)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ex_release_mutex_object);

	if (obj_desc->mutex.acquisition_depth == 0) {
		return (AE_NOT_ACQUIRED);
	}

	/* Match multiple Acquires with multiple Releases */

	obj_desc->mutex.acquisition_depth--;
	if (obj_desc->mutex.acquisition_depth != 0) {

		/* Just decrement the depth and return */

		return_ACPI_STATUS(AE_OK);
	}

	if (obj_desc->mutex.owner_thread) {

		/* Unlink the mutex from the owner's list */

		acpi_ex_unlink_mutex(obj_desc);
		obj_desc->mutex.owner_thread = NULL;
	}

	/* Release the mutex, special case for Global Lock */

	if (obj_desc == acpi_gbl_global_lock_mutex) {
		status = acpi_ev_release_global_lock();
	} else {
		acpi_os_release_mutex(obj_desc->mutex.os_mutex);
	}

	/* Clear mutex info */

	obj_desc->mutex.thread_id = NULL;
	return_ACPI_STATUS(status);
}


acpi_status
acpi_ex_release_mutex(union acpi_operand_object *obj_desc,
		      struct acpi_walk_state *walk_state)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ex_release_mutex);

	if (!obj_desc) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	/* The mutex must have been previously acquired in order to release it */

	if (!obj_desc->mutex.owner_thread) {
		ACPI_ERROR((AE_INFO,
			    "Cannot release Mutex [%4.4s], not acquired",
			    acpi_ut_get_node_name(obj_desc->mutex.node)));
		return_ACPI_STATUS(AE_AML_MUTEX_NOT_ACQUIRED);
	}

	/*
	 * The Mutex is owned, but this thread must be the owner.
	 * Special case for Global Lock, any thread can release
	 */
	if ((obj_desc->mutex.owner_thread->thread_id !=
	     walk_state->thread->thread_id)
	    && (obj_desc != acpi_gbl_global_lock_mutex)) {
		ACPI_ERROR((AE_INFO,
			    "Thread %lX cannot release Mutex [%4.4s] acquired by thread %lX",
			    (unsigned long)walk_state->thread->thread_id,
			    acpi_ut_get_node_name(obj_desc->mutex.node),
			    (unsigned long)obj_desc->mutex.owner_thread->
			    thread_id));
		return_ACPI_STATUS(AE_AML_NOT_OWNER);
	}

	/* Must have a valid thread ID */

	if (!walk_state->thread) {
		ACPI_ERROR((AE_INFO,
			    "Cannot release Mutex [%4.4s], null thread info",
			    acpi_ut_get_node_name(obj_desc->mutex.node)));
		return_ACPI_STATUS(AE_AML_INTERNAL);
	}

	/*
	 * The sync level of the mutex must be less than or equal to the current
	 * sync level
	 */
	if (obj_desc->mutex.sync_level > walk_state->thread->current_sync_level) {
		ACPI_ERROR((AE_INFO,
			    "Cannot release Mutex [%4.4s], SyncLevel mismatch: mutex %d current %d",
			    acpi_ut_get_node_name(obj_desc->mutex.node),
			    obj_desc->mutex.sync_level,
			    walk_state->thread->current_sync_level));
		return_ACPI_STATUS(AE_AML_MUTEX_ORDER);
	}

	status = acpi_ex_release_mutex_object(obj_desc);

	if (obj_desc->mutex.acquisition_depth == 0) {

		/* Restore the original sync_level */

		walk_state->thread->current_sync_level =
		    obj_desc->mutex.original_sync_level;
	}
	return_ACPI_STATUS(status);
}


void acpi_ex_release_all_mutexes(struct acpi_thread_state *thread)
{
	union acpi_operand_object *next = thread->acquired_mutex_list;
	union acpi_operand_object *obj_desc;

	ACPI_FUNCTION_ENTRY();

	/* Traverse the list of owned mutexes, releasing each one */

	while (next) {
		obj_desc = next;
		next = obj_desc->mutex.next;

		obj_desc->mutex.prev = NULL;
		obj_desc->mutex.next = NULL;
		obj_desc->mutex.acquisition_depth = 0;

		/* Release the mutex, special case for Global Lock */

		if (obj_desc == acpi_gbl_global_lock_mutex) {

			/* Ignore errors */

			(void)acpi_ev_release_global_lock();
		} else {
			acpi_os_release_mutex(obj_desc->mutex.os_mutex);
		}

		/* Mark mutex unowned */

		obj_desc->mutex.owner_thread = NULL;
		obj_desc->mutex.thread_id = NULL;

		/* Update Thread sync_level (Last mutex is the important one) */

		thread->current_sync_level =
		    obj_desc->mutex.original_sync_level;
	}
}
