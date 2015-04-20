/*
 * Contributed to the EUDAT DSI interface project by
 * Vladimir Mencl <vladimir.mencl@canterbury.ac.nz> (University of Canterbury, New Zealand)
 *
 * Existing project licenses apply.
 *
 * This module file has the core logic of talking to the iRODS API.
 *
 */

#ifdef IRODS_HEADER_HPP
  #include "rodsClient.hpp"
#else
  #include "rodsClient.h"
#endif
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "libirodsmap.h"

/** Open an iRODS connection */
int libirodsmap_connect(rcComm_t ** rcComm_out) {
    rodsEnv myRodsEnv;
    rErrMsg_t errMsg;
    int rc = 0;
    rcComm_t * rcComm = NULL;

    rc = getRodsEnv(&myRodsEnv);
    if (rc != 0) {
        libirodsmap_log(IRODSMAP_LOG_ERR, "libirodsmap_connect: getRodsEnv failed: %s%d\n", "", rc);
        goto connect_error;
    };

    rcComm = rcConnect(myRodsEnv.rodsHost, myRodsEnv.rodsPort, myRodsEnv.rodsUserName, myRodsEnv.rodsZone, 0, &errMsg);
    if (rcComm == NULL) {
        libirodsmap_log(IRODSMAP_LOG_ERR, "libirodsmap_connect: rcConnect failed ignore %s\n", errMsg.msg, 0);
        goto connect_error;
    };
    libirodsmap_log(IRODSMAP_LOG_DEBUG, "libirodsmap_connect: connected to iRODS server (%s:%d)\n", myRodsEnv.rodsHost, myRodsEnv.rodsPort);

#ifdef IRODS_HEADER_HPP
    rc = clientLogin(rcComm, NULL, NULL);
#else
    rc = clientLogin(rcComm);
#endif
    if (rc != 0) {
        libirodsmap_log(IRODSMAP_LOG_ERR,"libirodsmap_connect: clientLogin failed: %s%d\n", "", rc);
        goto connect_error;
    };


connect_error:
    if (rc != 0) {
       // cleanup
       if (rcComm != NULL) {
           rcDisconnect(rcComm);
           rcComm = NULL;
       };

    } else {
       *rcComm_out = rcComm;
    }
    return rc;
}

/** Add to inxValPair a SQL condition for equality on column inx to value value_to_check */
int libirodsmap_add_sqlcond(inxValPair_t *inxValPair, int inx, const char *value_to_check) {
	char * condStr;

	// NOTE: no need to check for special characters ("'") - even with
	// plain substitution, rcGenQuery interprets only the leading and
	// trailing "'" as special and correctly handles queries with DNs
	// containing "'" (e.g., CN=John O'Brien)
	int extra_chars = 3; // we use "='%s'"
	condStr = malloc(strlen(value_to_check)+extra_chars+1);
	if (condStr == NULL) return ENAMETOOLONG;

	sprintf(condStr,"='%s'",value_to_check);
	//libirodsmap_log(IRODSMAP_LOG_DEBUG, "libirodsmap_query_dn: DEBUG: adding condition %s (column %d)\n", condStr, inx);

    // Useful helper method:
    // int addInxVal (inxValPair_t *inxValPair, int inx, char *value);
    // NOTE: string passed to this method gets copied with strdup -
    // and we are still responsible for the pointer we pass

    addInxVal(inxValPair, inx, condStr);
    free(condStr);
    return 0;
}

/** Query the iCAT for the username+zone associated with a DN (optionally providing a desired username+zone)
 *
 * The SQL equivalent of the query is:
 *
 * select user_name, user_zone from r_user_main, r_user_auth where r_user_main.user_id = r_user_auth.user_id and user_auth_name = '/C=XX/O=YYY/CN=Example User';
 *
 * and iquest is:
 *
 * iquest "select USER_NAME, USER_ZONE where USER_DN = '/C=XX/O=YYY/CN=Example User'"
 *
 * Optionally (if desired_username+zone is specified) extended to:
 *
 * iquest "select USER_NAME, USER_ZONE where USER_DN = '/C=XX/O=YYY/CN=Example User' AND USER_NAME = 'example.user' AND USER_ZONE = 'ExampleZone'"
 *
 */
int libirodsmap_query_dn(rcComm_t * rcComm, const char * dn, char ** user, char ** zone, const char * desired_user, const char * desired_zone) {
    int rc = 0;

    genQueryInp_t *qi = NULL;
    genQueryOut_t *qo = NULL;

    // prepare query input in qi
    qi = calloc(1, sizeof(genQueryInp_t));
    if (qi == NULL) {
    	rc = ENAMETOOLONG;
    	goto query_error;
    }

    qi->maxRows = 10; // must specify non-zero - but really only want one result

    // Prepare qi->selectInp - output field selection
    // now I need to request USER_NAME in selectInp (COL_USER_NAME, COL_USER_ZONE) - with option 0 (no ORDER BY flags)
    // Useful helper method:
    // int addInxIval (inxIvalPair_t *inxIvalPair, int inx, int value);
    addInxIval(&qi->selectInp, COL_USER_NAME, 0);
    addInxIval(&qi->selectInp, COL_USER_ZONE, 0);

    // Specify condition with DN in sqlCondInp (COL_USER_DN)
    if ( (rc = libirodsmap_add_sqlcond(&qi->sqlCondInp, COL_USER_DN, dn)) != 0) goto query_error;

    // Optionally, if desired_user/zone have been set, add these as conditions too.
    if (desired_user != NULL)
    	if ( (rc = libirodsmap_add_sqlcond(&qi->sqlCondInp, COL_USER_NAME, desired_user)) != 0) goto query_error;
    if (desired_zone != NULL)
    	if ( (rc = libirodsmap_add_sqlcond(&qi->sqlCondInp, COL_USER_ZONE, desired_zone)) != 0) goto query_error;

    // invoke: int rcGenQuery (rcComm_t *conn, genQueryInp_t *genQueryInp, genQueryOut_t **genQueryOut)
    rc = rcGenQuery(rcComm, qi, &qo);

    // check genquery result
    if (rc != 0) {
    	// if the error is not CAT_NO_ROWS_FOUND, log it
        if (rc != CAT_NO_ROWS_FOUND)
            libirodsmap_log(IRODSMAP_LOG_ERR,"libirodsmap_query_dn: rcGenQuery failed: %s%d\n", "", rc);
        else
        	libirodsmap_log(IRODSMAP_LOG_INFO,"libirodsmap_query_dn: DN was not found: %s (rc %d)\n", dn, rc);

        goto query_error;
    };

    // rcGenQuery has succeeded.
	// We should see qo->rowCnt=1, qo->attriCnt=2
	if (qo->rowCnt==1 && qo->attriCnt==2) {
	    // qo->sqlResult is array sqlResult[attriCnt] where .value has values for
	    // all rows (seek in the string with row number - but we don't have to with just one row)
	    int attrIdx;
	    *user=NULL;
	    *zone=NULL;
	    for (attrIdx=0; attrIdx < qo->attriCnt; attrIdx++) {
			if (qo->sqlResult[attrIdx].attriInx==COL_USER_NAME) {
				*user=strdup(qo->sqlResult[attrIdx].value);
			    libirodsmap_log(IRODSMAP_LOG_DEBUG, "libirodsmap_query_dn: rcGenQuery returned user %s (rc=%d)\n", *user, rc);
			};
			if (qo->sqlResult[attrIdx].attriInx==COL_USER_ZONE) {
				*zone=strdup(qo->sqlResult[attrIdx].value);
			    libirodsmap_log(IRODSMAP_LOG_DEBUG, "libirodsmap_query_dn: rcGenQuery returned zone %s (rc=%d)\n", *zone, rc);
			};
	    };
	    // last check: if both user and zone are set.
	    // If not, either something went wrong with the query, or stdup failed
	    if (*user == NULL || *zone == NULL) {
	    	rc = ENAMETOOLONG;
	    	//free what has been allocated
	    	if (*user != NULL) { free(*user); *user=NULL; };
	    	if (*zone != NULL) { free(*zone); *zone=NULL; };
	    }
	    // in this case, rc == 0
	} else {
		// if we did not get exactly one row with two attributes, we mark it as a failure
		rc = CAT_NO_ROWS_FOUND;
		libirodsmap_log(IRODSMAP_LOG_ERR,"libirodsmap_query_dn: rcGenQuery returned %s%d rows\n", "", qo->rowCnt);
		libirodsmap_log(IRODSMAP_LOG_ERR,"libirodsmap_query_dn: rcGenQuery returned %s%d attributes\n", "", qo->attriCnt);
	}
	// only if rcGenQuery was successful we free genQuery_out
	// And we use freeGenQueryOut which dives into the data structures
	// (and also sets the pointer to NULL - that's why it's passed by reference)
	freeGenQueryOut(&qo);

 query_error:
	/* free input structure
	 * use these methods to release the allocations made by addInxIval/addInxVal
	 * int clearInxIval (inxIvalPair_t *inxIvalPair);
	 * int clearInxVal (inxValPair_t *inxValPair);
	 */
    if (qi != NULL) {
		clearInxIval(&qi->selectInp);
		clearInxVal(&qi->sqlCondInp);
		free(qi);
    }


    return rc;
}


int libirodsmap_exec_command(rcComm_t * rcComm, const char * dnCommand, const char * dn)
{
    execCmd_t *execCmd = NULL;
    execCmdOut_t *execCmdOut = NULL;
    int rc = 0;

    execCmd = calloc(1, sizeof(execCmd_t));
    if (execCmd == NULL) {
    	rc = ENAMETOOLONG;
    	goto exec_command_error;
    }

    if ( (strlen(dnCommand)+1) <= sizeof(execCmd->cmd) )
        strcpy(execCmd->cmd,dnCommand);
    else {
    	rc = ENAMETOOLONG;
        libirodsmap_log(IRODSMAP_LOG_ERR, "libirodsmap_exec_command: " IRODS_DN_COMMAND " too long: %s\n", dnCommand, 0);
    	goto exec_command_error;
    }
    if ( (strlen(dn)+1) <= sizeof(execCmd->cmdArgv) )
        strcpy(execCmd->cmdArgv,dn);
    else {
    	rc = ENAMETOOLONG;
        libirodsmap_log(IRODSMAP_LOG_ERR, "libirodsmap_exec_command: DN too long: %s\n", dn, 0);
    	goto exec_command_error;
    }

    // Invoke: int rcExecCmd (rcComm_t *conn, execCmd_t *execCmdInp, execCmdOut_t **execCmdOut)
    libirodsmap_log(IRODSMAP_LOG_INFO,"libirodsmap_exec_command: invoking rsExecCmd with command %s\n", dnCommand, 0);
    libirodsmap_log(IRODSMAP_LOG_INFO,"libirodsmap_exec_command: invoking rsExecCmd with DN %s\n", dn, 0);
    rc = rcExecCmd (rcComm, execCmd, &execCmdOut);
    if (rc != 0)
    	libirodsmap_log(IRODSMAP_LOG_ERR,"libirodsmap_exec_command: rsExecCmd returned %s%d\n", "", rc);

exec_command_error:
    // cleanup
	if (execCmd != NULL) { free(execCmd); execCmd = NULL; };
	if (execCmdOut != NULL) { free(execCmdOut); ; execCmdOut = NULL; };

	return rc;
}



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

int get_irods_mapping(const char * dn, char ** user, char ** zone, const char * desired_user, const char * desired_zone)
{
    rcComm_t *rcComm = NULL;
    int rc = 0;

    if ( (rc=libirodsmap_connect(&rcComm)) != 0 ) goto irods_mapping_error;

    rc = libirodsmap_query_dn(rcComm, dn, user, zone, desired_user, desired_zone);
    if ( rc !=0 && rc != CAT_NO_ROWS_FOUND) goto irods_mapping_error;

    // If the user was not found and if configured to run a script, invoke the command now and try querying again
    if ( rc == CAT_NO_ROWS_FOUND ) {
    	char * dnCommand = getenv(IRODS_DN_COMMAND);
    	if (dnCommand != NULL) {
    	    if ( (rc=libirodsmap_exec_command(rcComm, dnCommand, dn)) != 0 ) goto irods_mapping_error;
    	    // try one more time the query - if the command changed something and the DN has a mapping now
    	    rc = libirodsmap_query_dn(rcComm, dn, user, zone, desired_user, desired_zone);
    	    if ( rc !=0 && rc != CAT_NO_ROWS_FOUND) goto irods_mapping_error;
    	}
    }
    // now, all attempts are completed (and none of them ran into an error)
    // rc is either 0 (user+zone found) or CAT_NO_ROWS_FOUND
    // nothing to do: user+zone have already been set
    // so just return rc

irods_mapping_error:
	// cleanup
	if (rcComm != NULL) {
		// disconnect
		rcDisconnect(rcComm);
		rcComm = NULL;
	}
    return rc;
}
