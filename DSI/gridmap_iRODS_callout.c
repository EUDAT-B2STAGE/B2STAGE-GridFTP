/*
 * Contributed to the EUDAT DSI interface project by
 * Vladimir Mencl <vladimir.mencl@canterbury.ac.nz> (University of Canterbury, New Zealand)
 *
 * This module file implements the required interface of a gridmap callout function.
 * The code in this module heavily borrows from the
 * gridmap_verify_myproxy_callout, Globus Toolkit 6.0,
 * Copyright 1999-2006 University of Chicago
 *
 * As such, this code is distributed under the Apache License 2.0, as used by
 * the Globus Toolkit project.
 *
 */

#include "globus_common.h"
#include "gssapi.h"
#include "globus_gss_assist.h"
#include "globus_gsi_credential.h"
#include "globus_gridmap_callout_error.h"

#include "globus_gridftp_server.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>


#include <stdlib.h>
#include <openssl/ssl.h>

#include "libirodsmap.h"

#include <sys/types.h>
#include <sys/wait.h>

#define IRODS_PREMAP_SCRIPT "IRODS_PREMAP_SCRIPT"


/* Get the subject from the globus context */
globus_result_t
gridmap_iRODS_callout_get_subject(
    gss_ctx_id_t                        context,
    char **                             subject)
{
    gss_name_t                          peer;
    gss_buffer_desc                     peer_name_buffer;
    OM_uint32                           major_status;
    OM_uint32                           minor_status;
    int                                 initiator;
    globus_result_t                     result = GLOBUS_SUCCESS;

    major_status = gss_inquire_context(&minor_status,
                                       context,
                                       GLOBUS_NULL,
                                       GLOBUS_NULL,
                                       GLOBUS_NULL,
                                       GLOBUS_NULL,
                                       GLOBUS_NULL,
                                       &initiator,
                                       GLOBUS_NULL);

    if(GSS_ERROR(major_status))
    {
        GLOBUS_GRIDMAP_CALLOUT_GSS_ERROR(result, major_status, minor_status);
        goto error;
    }

    major_status = gss_inquire_context(&minor_status,
                                       context,
                                       initiator ? GLOBUS_NULL : &peer,
                                       initiator ? &peer : GLOBUS_NULL,
                                       GLOBUS_NULL,
                                       GLOBUS_NULL,
                                       GLOBUS_NULL,
                                       GLOBUS_NULL,
                                       GLOBUS_NULL);

    if(GSS_ERROR(major_status))
    {
        GLOBUS_GRIDMAP_CALLOUT_GSS_ERROR(result, major_status, minor_status);
        goto error;
    }

    major_status = gss_display_name(&minor_status,
                                    peer,
                                    &peer_name_buffer,
                                    GLOBUS_NULL);

    if(GSS_ERROR(major_status))
    {
        GLOBUS_GRIDMAP_CALLOUT_GSS_ERROR(result, major_status, minor_status);
        gss_release_name(&minor_status, &peer);
        goto error;
    }


    *subject = globus_libc_strdup(peer_name_buffer.value);
    gss_release_buffer(&minor_status, &peer_name_buffer);
    gss_release_name(&minor_status, &peer);

    return GLOBUS_SUCCESS;

error:
    return result;
}

globus_result_t
gridmap_iRODS_mapuser(char * subject, char ** found_identity, char * desired_identity) {
  char * user = NULL;
  char * zone = NULL;
  char * desired_user = NULL;
  char * desired_zone = NULL;
  char * identity = NULL;
  int result = GLOBUS_SUCCESS;

  if (desired_identity != NULL) {

      // split the desired_identity into user#zone
      // if the '#' separator is present
      char * separator_ptr;
      if ( (separator_ptr = strchr(desired_identity,'#')) != NULL ) {
    	  // NOTE: these strdup calls are not checked
          desired_user = strndup(desired_identity, separator_ptr - desired_identity);
          desired_zone = strdup(separator_ptr + 1);
      } else {
          desired_user = strdup(desired_identity);
          desired_zone = NULL;
      };
  };

  if ( get_irods_mapping(subject, &user, &zone, desired_user, desired_zone ) == 0) {
    identity=calloc(1, strlen(user)+1+strlen(zone)+1);
    if (identity != NULL) {
        strcpy(identity,user);
        strcat(identity,"#");
        strcat(identity,zone);
        *found_identity=identity;
        result = GLOBUS_SUCCESS;
    } else {
    	result = ENAMETOOLONG;
    }
    free(user);
    free(zone);
  } else {
    result = GLOBUS_FAILURE;
  };

  // cleanup - in case we created these
  if (desired_user != NULL) free(desired_user);
  if (desired_zone != NULL) free(desired_zone);
  return result;
}

globus_result_t
gridmap_iRODS_callout(
    va_list                             ap)
{
    gss_ctx_id_t                        context;
    char *                              service;
    char *                              desired_identity;
    char *                              identity_buffer;
    char *                              found_identity = NULL;
    char *                              subject = NULL;
    unsigned int                        buffer_length;
    globus_result_t                     result = GLOBUS_SUCCESS;
    char *                              shared_user_cert = NULL;
    int                                 rc;

    globus_gfs_log_message(GLOBUS_GFS_LOG_DUMP, "gridmap_iRODS_callout starting\n");

    rc = globus_module_activate(GLOBUS_GSI_GSS_ASSIST_MODULE);
    rc = globus_module_activate(GLOBUS_GSI_GSSAPI_MODULE);
    rc = globus_module_activate(GLOBUS_GRIDMAP_CALLOUT_ERROR_MODULE);

    context = va_arg(ap, gss_ctx_id_t);
    service = va_arg(ap, char *);
    desired_identity = va_arg(ap, char *);
    identity_buffer = va_arg(ap, char *);
    buffer_length = va_arg(ap, unsigned int);

    globus_gfs_log_message(GLOBUS_GFS_LOG_DUMP, "gridmap_iRODS_callout: context = %p\n", context);
    globus_gfs_log_message(GLOBUS_GFS_LOG_DUMP, "gridmap_iRODS_callout: service = %s\n", service);
    globus_gfs_log_message(GLOBUS_GFS_LOG_DUMP, "gridmap_iRODS_callout: desired_identity = %s\n", desired_identity);
    globus_gfs_log_message(GLOBUS_GFS_LOG_DUMP, "gridmap_iRODS_callout: identity_buffer = %s\n", identity_buffer);
    globus_gfs_log_message(GLOBUS_GFS_LOG_DUMP, "gridmap_iRODS_callout: buffer_length = %d\n", buffer_length);

    if(strcmp(service, "sharing") == 0)
    {
        shared_user_cert = va_arg(ap, char *);

        result = globus_gsi_cred_read_cert_buffer(
            shared_user_cert, NULL, NULL, NULL, &subject);
        if(result != GLOBUS_SUCCESS)
        {
            GLOBUS_GRIDMAP_CALLOUT_ERROR(
                result,
                GLOBUS_GRIDMAP_CALLOUT_GSSAPI_ERROR,
                ("Could not extract shared user identity."));
            goto error;
        }
	globus_gfs_log_message(GLOBUS_GFS_LOG_DUMP, "gridmap_iRODS_callout: sharing: subject = %s\n", subject);
    }
    else
    {
        // Extract user identity
        // Reuse code from gridmap_callout_verify_myproxy
        result = gridmap_iRODS_callout_get_subject(context, &subject);
        if (result == GLOBUS_SUCCESS)
	    globus_gfs_log_message(GLOBUS_GFS_LOG_DUMP, "gridmap_iRODS_callout: context: subject = %s\n", subject);

    }

    if(result != GLOBUS_SUCCESS || subject == NULL)
    {
        GLOBUS_GRIDMAP_CALLOUT_ERROR(
            result,
            GLOBUS_GRIDMAP_CALLOUT_GSSAPI_ERROR,
            ("Could not extract user identity."));
        goto error;
    }
	
	// Calling the pre-map script (passed in $IRODS_PREMAP_SCRIPT), that allows verifying the user online prior mapping
	// The mechanism was implemented for EUDAT-PRACE integration and the script for that integration is:
	// B2SAFE-core/scripts/authN_and_authZ/irods_user_sync.py
	// Michal Jankowski PSNC, 03.2017
	const char* premap_script = getenv(IRODS_PREMAP_SCRIPT);
	if(premap_script != NULL)
	{
		int comand_len = strlen(premap_script) + strlen(subject) + 8;
		char* command = malloc(comand_len);
		if(command == NULL)
        {
		   GLOBUS_GRIDMAP_CALLOUT_ERROR(
               result,
               GLOBUS_GRIDMAP_CALLOUT_GSSAPI_ERROR,
               ("Pre-map script cannot be called, memory allocation problem."));
            goto error;
	    }
		
		snprintf(command, comand_len, "%s -d \"%s\"", premap_script, subject);
		globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "Calling pre-map command %s.\n", command);
		int command_result = system(command);
		free(command);
		
		if(command_result != 0)
        {
		
		   GLOBUS_GRIDMAP_CALLOUT_ERROR(
               result,
               GLOBUS_GRIDMAP_CALLOUT_GSSAPI_ERROR,
               ("Pre-map script failed."));
            goto error;
	    }
	 }

    // Perform the mapping now - set found_identity
    result = gridmap_iRODS_mapuser(subject, &found_identity, desired_identity);
    if(result == GLOBUS_SUCCESS)
    {
	globus_gfs_log_message(GLOBUS_GFS_LOG_DUMP, "gridmap_iRODS_callout: identity: %s\n", found_identity);
        if(desired_identity && strcmp(found_identity, desired_identity) != 0)
        {
            GLOBUS_GRIDMAP_CALLOUT_ERROR(
                result,
                GLOBUS_GRIDMAP_CALLOUT_LOOKUP_FAILED,
                ("Credentials specify id of %s, can not allow id of %s.\n",
                 found_identity, desired_identity));
            globus_free(found_identity);
            goto error;
        }
    }
    else
    {
        result = GLOBUS_SUCCESS;
        /* proceed with gridmap lookup */
        if(desired_identity == NULL)
        {
            rc = globus_gss_assist_gridmap(subject, &found_identity);
            if(rc != 0)
            {
                GLOBUS_GRIDMAP_CALLOUT_ERROR(
                    result,
                    GLOBUS_GRIDMAP_CALLOUT_LOOKUP_FAILED,
                    ("Could not map %s\n", subject));
                goto error;
            }
        }
        else
        {
            rc = globus_gss_assist_userok(subject, desired_identity);
            if(rc != 0)
            {
                GLOBUS_GRIDMAP_CALLOUT_ERROR(
                    result,
                    GLOBUS_GRIDMAP_CALLOUT_LOOKUP_FAILED,
                    ("Could not map %s to %s\n",
                     subject, desired_identity));
                goto error;
            }
            found_identity = globus_libc_strdup(desired_identity);
        }
    }

    if(found_identity)
    {
        if(strlen(found_identity) + 1 > buffer_length)
        {
            GLOBUS_GRIDMAP_CALLOUT_ERROR(
                result,
                GLOBUS_GRIDMAP_CALLOUT_BUFFER_TOO_SMALL,
                ("Local identity length: %d Buffer length: %d\n",
                 strlen(found_identity), buffer_length));
        }
        else
        {
            strcpy(identity_buffer, found_identity);
        }
        globus_free(found_identity);
    }


error:

    if(subject)
    {
        globus_free(subject);
    }

    globus_module_deactivate(GLOBUS_GRIDMAP_CALLOUT_ERROR_MODULE);
    globus_module_deactivate(GLOBUS_GSI_GSSAPI_MODULE);
    globus_module_deactivate(GLOBUS_GSI_GSS_ASSIST_MODULE);

    return result;
}

void libirodsmap_log(int level, const char * message, const char * param, int status) {
    globus_gfs_log_message(level, message, param, status);
}

