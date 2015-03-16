/*
 * Contributed to the EUDAT DSI interface project by
 * Vladimir Mencl <vladimir.mencl@canterbury.ac.nz> (University of Canterbury, New Zealand)
 *
 * Existing project licenses apply.
 *
 * This module file has the core logic of talking to the iRODS API.
 *
 */


#ifndef LIB_IRODS_MAP
#define LIB_IRODS_MAP 1

// Configuration parameters

// Name of environment variable holding the name of an iRODS
// server-side command to invoke when a DN match is not found
#define IRODS_DN_COMMAND "irodsDnCommand"


/** Get the iRODS account name based on a DN.
 *
 * This is a high-level routine that connects as rods and tries looking the name up based on the DN.
 *
 * If successful, the routine allocates the user and zone strings, which are then the callers responsibility to free.
 *
 * Params:
 *     dn - the DN to map
 *     user, zone: upon success, return a pointer to the mapped user and zone here.
 *                 The caller is responsible for freeing the pointers.
 *     desired_user, desired zone: if the user has a preferred mapping, search if this username already exists.
 *
 * Returns: 0 if the mapping was successfully obtained.  In this case, user and zone would be set.
 *          non-zero otherwise (including when a mapping did not exist.  The value would be CAT_NO_ROWS_FOUND in thic case)
 */

int get_irods_mapping(const char * dn, char ** user, char ** zone, const char * desired_user, const char * desired_zone);

/** Logging function that can translate to globus loggign or stderr
 * For simplicity, a single fucntion taking a string and an int parameter.
 * String is passed first (and can be empty if not needed).
 * Integer can be omitted from the formatting message.
 */

void libirodsmap_log(int level, const char * message, const char * param, int status);

/* Log levels for logging - based on Globus values in globus_gridftp_server.h */
#define IRODSMAP_LOG_ERR    1
#define IRODSMAP_LOG_WARN   2
#define IRODSMAP_LOG_INFO   8
#define IRODSMAP_LOG_DEBUG 16 // GLOBUS_GFS_LOG_DUMP

#endif
