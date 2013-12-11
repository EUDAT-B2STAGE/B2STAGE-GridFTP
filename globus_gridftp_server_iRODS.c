/*
 * Copyright (c) 2013 CINECA (www.hpc.cineca.it)
 *
 * Copyright (c) 1999-2006 University of Chicago
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 * Globus DSI to manage data on iRODS.
 *
 * Author: Roberto Mucci - SCAI - CINECA
 * Email:  hpc-service@cineca.it
 *
 */


#include "globus_gridftp_server.h"
#include "rodsClient.h"
#include <stdio.h> 
#include <time.h>
#include <unistd.h>

#define MAX_DATA_SIZE 1024

#ifndef IRODS_MAPS_PATH
  #define IRODS_MAPS_PATH = ""
#endif

static int                              iRODS_l_dev_wrapper = 10;
struct iRODS_Resource
{
      char * path;
      char * resource;
};

struct iRODS_Resource iRODS_Resource_struct = {NULL,NULL};

GlobusDebugDefine(GLOBUS_GRIDFTP_SERVER_IRODS); 
static
globus_version_t local_version =
{
    0, /* major version number */
    1, /* minor version number */
    1369393102,
    0 /* branch ID */
};

int
iRODS_l_reduce_path(
    char *                              path)
{
    char *                              ptr;
    int                                 len;
    int                                 end;

    len = strlen(path);

    while(len > 1 && path[len-1] == '/')
    {
        len--;
        path[len] = '\0';
    }
    end = len-2;
    while(end >= 0)
    {
        ptr = &path[end];
        if(strncmp(ptr, "//", 2) == 0)
        {
            memmove(ptr, &ptr[1], len - end);
            len--;
        }
        end--;
    }
    return 0;
}

typedef struct globus_l_iRODS_read_ahead_s
{
    struct globus_l_gfs_iRODS_handle_s *  iRODS_handle;
    globus_off_t                        offset;
    globus_size_t                       length;
    globus_byte_t                       buffer[1];
} globus_l_iRODS_read_ahead_t;

static
int
iRODS_l_filename_hash(
    char *                              string)
{
    int                                 rc;
    unsigned long                       h = 0;
    unsigned long                       g;
    char *                              key;

    if(string == NULL)
    {
        return 0;
    }

    key = (char *) string;

    while(*key)
    {
        h = (h << 4) + *key++;
        if((g = (h & 0xF0UL)))
        {
            h ^= g >> 24;
            h ^= g;
        }
    }

    rc = h % 2147483647;
    return rc;
}

static
void 
iRODS_disconnect(
    rcComm_t *                           conn)

{
    globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "iRODS: disconnected.\n");
    rcDisconnect(conn);
}

static
char *
iRODS_getUserName(
    char *                              DN)
{
    char *DN_Read = NULL;
    char *iRODS_user_name = NULL;
    char *search = ";";

    char filename[1024];
    snprintf( filename, sizeof(filename), "%s/irodsUserMap.conf",IRODS_MAPS_PATH);

    FILE *file = fopen ( filename, "r" );
    if ( file != NULL )
    {
        char line [ 256 ]; /* or other suitable maximum line size */
        while ( fgets ( line, sizeof line, file ) != NULL ) /* read a line */
        {
            // Token will point to the part before the ;.
            DN_Read = strtok(line, search);
            if ( strcmp(DN, DN_Read) == 0)
            {
                iRODS_user_name = strtok(NULL, search);
                unsigned int len = strlen(iRODS_user_name);
                if (iRODS_user_name[len - 1] == '\n')
                {
                    iRODS_user_name[len - 1] = '\0'; //Remove EOF 
                }
                globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "iRODS: User found in irodsUserMap.conf: DN = %s, iRODS user = %s.\n", DN, iRODS_user_name);
                break;
            }
        }
        fclose ( file );
    } 
    return iRODS_user_name; 
}


static
void
iRODS_getResource(
    char *                         destinationPath)
{
    char *path_Read = NULL;
    char *iRODS_res = NULL;
    char *search = ";";
   
    char filename[1024];
    snprintf( filename, sizeof(filename), "%s/irodsResourceMap.conf", IRODS_MAPS_PATH);

    FILE *file = fopen ( filename, "r" );
    if ( file != NULL )
    {
        char line [ 256 ]; /* or other suitable maximum line size */
        while ( fgets ( line, sizeof line, file ) != NULL ) /* read a line */
        {
            // Token will point to the part before the ;.
            path_Read = strtok(line, search);
 
            if (strncmp(path_Read, destinationPath, strlen(path_Read)) == 0)
            {
	            //found the resource
                iRODS_res = strtok(NULL, search);
                unsigned int len = strlen(iRODS_res);
                if (iRODS_res[len - 1] == '\n')
                {
                    iRODS_res[len - 1] = '\0'; //Remove EOF 
                }
                globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "iRODS: Resource found in %s: destinationPath = %s, iRODS resource = %s.\n", filename, destinationPath, iRODS_res);
                
                iRODS_Resource_struct.resource =  strdup(iRODS_res); 
                iRODS_Resource_struct.path = strdup(path_Read);
                break;
            }
        }
        fclose ( file );
    }
    else
    {
        globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "iRODS: irodsResourceMap.conf not found in %s\n", filename);
    }  

}


static
int
iRODS_l_stat1(
    rcComm_t *                          conn,
    globus_gfs_stat_t *                 stat_out,
    char *                              start_dir)
{
    int                                 status;
    char *                              tmp_s;
    char *                              rsrcName;
    char *                              fname;

    collHandle_t collHandle;
    int queryFlags;
    queryFlags = DATA_QUERY_FIRST_FG | VERY_LONG_METADATA_FG | NO_TRIM_REPL_FG;
    status = rclOpenCollection (conn, start_dir, queryFlags,  &collHandle);
    if (status >= 0)
    {
    
        globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "iRODS: found collection %s.\n", start_dir);
        rsrcName = (char*) start_dir;
        memset(stat_out, '\0', sizeof(globus_gfs_stat_t));
        fname = rsrcName ? rsrcName : "(null)";
        tmp_s = strrchr(fname, '/');
        if(tmp_s != NULL) fname = tmp_s + 1;
        stat_out->ino = iRODS_l_filename_hash(rsrcName);
        stat_out->name = strdup(fname);
        stat_out->nlink = 0;
        stat_out->uid = getuid();
        stat_out->gid = getgid();
        stat_out->size = 0;
        stat_out->dev = iRODS_l_dev_wrapper++;
        stat_out->mode =
            S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR |
            S_IROTH | S_IXOTH | S_IRGRP | S_IXGRP;
    }
    else
    {
        char sizeStr[NAME_LEN];
        dataObjInp_t dataObjInp; 
        rodsObjStat_t *rodsObjStatOut = NULL; 
        bzero (&dataObjInp, sizeof (dataObjInp)); 
        rstrcpy (dataObjInp.objPath, start_dir, MAX_NAME_LEN); 
        status = rcObjStat (conn, &dataObjInp, &rodsObjStatOut); 
        if (status >= 0) 
        { 
            globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "iRODS: found data object %s.\n", start_dir);      
            memset(stat_out, '\0', sizeof(globus_gfs_stat_t));
            stat_out->symlink_target = NULL;
            stat_out->name = strdup(start_dir);
            stat_out->nlink = 0;
            stat_out->uid = getuid();
            stat_out->gid = getgid();
            snprintf (sizeStr, NAME_LEN, "%lld", rodsObjStatOut->objSize);
            stat_out->size = sizeStr;

            time_t realTime = atol(rodsObjStatOut->modifyTime);
            stat_out->ctime = realTime;
            stat_out->mtime = realTime;
            stat_out->atime = realTime;
        }
        freeRodsObjStat (rodsObjStatOut); 
    }

    if(status == -808000)
    {
       globus_gfs_log_message(GLOBUS_GFS_LOG_INFO,"iRODS: object or collection called: %s not found\n", start_dir);
    }
    return status;
}



static
int
iRODS_l_stat_dir(
    rcComm_t*                           conn,
    globus_gfs_stat_t **                out_stat,
    int *                               out_count,
    char *                              start_dir,
    char *                              username)
{
    int                                 status;
    char *                              tmp_s;
    globus_gfs_stat_t *                 stat_array = NULL;
    int                                 stat_count = 0;
    int                                 stat_ndx = 0;
    
    collHandle_t collHandle;
    collEnt_t collEnt;
    int queryFlags;

    queryFlags = DATA_QUERY_FIRST_FG | VERY_LONG_METADATA_FG | NO_TRIM_REPL_FG;
    status = rclOpenCollection (conn, start_dir, queryFlags,  &collHandle);
    
    if (status < 0) {
        globus_gfs_log_message(GLOBUS_GFS_LOG_INFO,"iRODS: rclOpenCollection of %s error. status = %d", start_dir, status);
        return status;
    }

    while ((status = rclReadCollection (conn, &collHandle, &collEnt)) >= 0)
    {
        stat_count++;
        stat_array = (globus_gfs_stat_t *) globus_realloc(stat_array, stat_count * sizeof(globus_gfs_stat_t));

        if (collEnt.objType == DATA_OBJ_T) 
        {
		    memset(&stat_array[stat_ndx], '\0', sizeof(globus_gfs_stat_t));
		    stat_array[stat_ndx].symlink_target = NULL;
		    stat_array[stat_ndx].name = globus_libc_strdup(collEnt.dataName);
    	    stat_array[stat_ndx].nlink = 0;
	        stat_array[stat_ndx].uid = getuid();

	        //I could get unix uid from iRODS owner, but iRODS owner can not exist as unix user
	       	//so now the file owner is always the user who started the gridftp process
	       	//stat_array[stat_ndx].uid = getpwnam(ownerName)->pw_uid;
		    
            stat_array[stat_ndx].gid = getgid();
            stat_array[stat_ndx].size = collEnt.dataSize;

		    time_t realTime = atol(collEnt.modifyTime);
	    	stat_array[stat_ndx].ctime = realTime;
    	  	stat_array[stat_ndx].mtime = realTime;
        	stat_array[stat_ndx].atime = realTime;
        } 
        else
        {
            char * fname;
            fname = collEnt.collName ? collEnt.collName : "(null)";
            tmp_s = strrchr(fname, '/');
            if(tmp_s != NULL) fname = tmp_s + 1;
            if(strlen(fname) == 0)
            {
                //in iRODS empty dir collection is root dir
                fname = ".";
            }
                
            memset(&stat_array[stat_ndx], '\0', sizeof(globus_gfs_stat_t));
            stat_array[stat_ndx].ino = iRODS_l_filename_hash(collEnt.collName);
            stat_array[stat_ndx].name = strdup(fname);
            stat_array[stat_ndx].nlink = 0;
            stat_array[stat_ndx].uid = getuid();
            stat_array[stat_ndx].gid = getgid();
            stat_array[stat_ndx].size = 0;
            stat_array[stat_ndx].dev = iRODS_l_dev_wrapper++;
            stat_array[stat_ndx].mode = S_IFDIR | S_IRUSR | S_IWUSR | 
                S_IXUSR | S_IROTH | S_IXOTH | S_IRGRP | S_IXGRP;
        }
      stat_ndx++;
    }

    rclCloseCollection (&collHandle);

    //There should be at least one element (".")
    if (stat_ndx == 0)
    {
        stat_count = 1;
        stat_array = (globus_gfs_stat_t *) globus_calloc(
        stat_count, sizeof(globus_gfs_stat_t));
        stat_array[stat_ndx].ino = iRODS_l_filename_hash(start_dir);
        stat_array[stat_ndx].name = strdup(".");
        stat_array[stat_ndx].nlink = 0;
        stat_array[stat_ndx].uid = getuid();
        stat_array[stat_ndx].gid = getgid();
        stat_array[stat_ndx].size = 0;
        stat_array[stat_ndx].dev = iRODS_l_dev_wrapper++;
        stat_ndx++;
    }
    *out_stat = stat_array;
    *out_count = stat_count;

    if (status < 0 && status != -808000) {
        return (status);
    } else {
        return (0);
    }
}


/* 
*  the data structure representing the FTP session
*/
typedef struct globus_l_gfs_iRODS_handle_s
{
    rcComm_t *                          conn;
    int                                 stor_sys_type;
    int                                 fd;
    globus_mutex_t                      mutex;
    globus_gfs_operation_t              op;
    globus_bool_t                       done;
    globus_bool_t                       read_eof;
    int                                 outstanding;
    int                                 optimal_count;
    globus_size_t                       block_size;
    globus_result_t                     cached_res;
    globus_off_t                        blk_length;
    globus_off_t                        blk_offset;
    
    globus_fifo_t                       rh_q;
    
    char *                              hostname;
    char *                              port;
    
    char *                              zone;
    char *                              user;
    char *                              domain; 
    
    char *                              irods_dn;
  
} globus_l_gfs_iRODS_handle_t;


static
globus_bool_t
globus_l_gfs_iRODS_read_from_net(
    globus_l_gfs_iRODS_handle_t *         iRODS_handle);


static
globus_bool_t
globus_l_gfs_iRODS_send_next_to_client(
    globus_l_gfs_iRODS_handle_t *         iRODS_handle);

static
void
globus_l_gfs_iRODS_read_ahead_next(
    globus_l_gfs_iRODS_handle_t *         iRODS_handle);




/*
 *  utility function to make errors
 */
static
globus_result_t
globus_l_gfs_iRODS_make_error(
    const char *                        msg,
    int                                 status)
{
    char *errorSubName;
    char *errorName;    
    char *                              err_str;
    globus_result_t                     result;
    GlobusGFSName(globus_l_gfs_iRODS_make_error);
  
    errorName = rodsErrorName(status, &errorSubName);

    err_str = globus_common_create_string("iRODS Error: %s. %s: %s. status: %d.\n", msg, errorName, errorSubName, status);
    result = GlobusGFSErrorGeneric(err_str);
    free(err_str);

    return result;
}



/*************************************************************************
 *  start
 *  -----
 *  This function is called when a new session is initialized, ie a user 
 *  connectes to the server.  This hook gives the dsi an oppertunity to
 *  set internal state that will be threaded through to all other
 *  function calls associated with this session.  And an oppertunity to
 *  reject the user.
 *
 *  finished_info.info.session.session_arg should be set to an DSI
 *  defined data structure.  This pointer will be passed as the void *
 *  user_arg parameter to all other interface functions.
 * 
 *  NOTE: at nice wrapper function should exist that hides the details 
 *        of the finished_info structure, but it currently does not.  
 *        The DSI developer should jsut follow this template for now
 ************************************************************************/
static
void
globus_l_gfs_iRODS_start(
    globus_gfs_operation_t              op,
    globus_gfs_session_info_t *         session_info)
{
    globus_l_gfs_iRODS_handle_t *       iRODS_handle;
    globus_result_t                           result;
    char *                              tmp_str;
    globus_gfs_finished_info_t          finished_info;

    GlobusGFSName(globus_l_gfs_iRODS_start);

    rodsEnv myRodsEnv;
    char *user_name;
    int status;
    rErrMsg_t errMsg;

    iRODS_handle = (globus_l_gfs_iRODS_handle_t *)
        globus_malloc(sizeof(globus_l_gfs_iRODS_handle_t));

    if(iRODS_handle == NULL)
    {
        result = GlobusGFSErrorGeneric("iRODS start: malloc failed");
        goto error;
    }
    globus_mutex_init(&iRODS_handle->mutex, NULL);
    globus_fifo_init(&iRODS_handle->rh_q);    

    memset(&finished_info, '\0', sizeof(globus_gfs_finished_info_t));
    finished_info.type = GLOBUS_GFS_OP_SESSION_START;
    finished_info.result = GLOBUS_SUCCESS;
    finished_info.info.session.session_arg = iRODS_handle;
    finished_info.info.session.username = session_info->username;
    ///finished_info.info.session.home_dir = "/";
    
    status = getRodsEnv(&myRodsEnv);
    if (status < 0) {
        result = globus_l_gfs_iRODS_make_error("\'getRodsEnv\' failed.", status);
        goto rodsenv_error; 
    }
    
    iRODS_handle->hostname = myRodsEnv.rodsHost;
    iRODS_handle->port = (char *)myRodsEnv.rodsPort;
    iRODS_handle->zone = myRodsEnv.rodsZone;
    iRODS_handle->user = iRODS_getUserName(session_info->subject); //iRODS usernmae
    user_name = strdup(session_info->username); //Globus user name

    if (iRODS_handle->user == NULL)
    {
        iRODS_handle->user = session_info->username;
    }
 
    globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "iRODS: calling rcConnect(%s,%i,%s,%s)\n", iRODS_handle->hostname, iRODS_handle->port, iRODS_handle->user /*user_name*/, iRODS_handle->zone);
    iRODS_handle->conn = rcConnect(iRODS_handle->hostname, (int)iRODS_handle->port, iRODS_handle->user, iRODS_handle->zone, 0, &errMsg);
    if (iRODS_handle->conn == NULL) {
        tmp_str = globus_common_create_string("rcConnect failed::\n  '%s'. Host: '%s', Port: '%i', UserName '%s', Zone '%s'\n",errMsg.msg, iRODS_handle->hostname,
        iRODS_handle->port, iRODS_handle->user, iRODS_handle->zone);
        char *err_str = globus_common_create_string("iRODS \'rcConnect\' failed: %s\n", errMsg.msg);
        result = GlobusGFSErrorGeneric(err_str); 
        goto connect_error;
    }
    
    status = clientLogin(iRODS_handle->conn);
    if (status != 0) {
        result = globus_l_gfs_iRODS_make_error("\'clientLogin\' failed.", status);
        goto error;
    }

    globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "iRODS: connected.\n");




    free(user_name);
    finished_info.info.session.home_dir = globus_common_create_string("/%s/home/%s", iRODS_handle->zone, user_name);

    globus_gridftp_server_operation_finished(op, GLOBUS_SUCCESS, &finished_info);
    globus_free(finished_info.info.session.home_dir);
    return;

rodsenv_error:
connect_error:
error:
    globus_gridftp_server_operation_finished(
        op, result, &finished_info);
}

/*************************************************************************
 *  destroy
 *  -------
 *  This is called when a session ends, ie client quits or disconnects.
 *  The dsi should clean up all memory they associated wit the session
 *  here. 
 ************************************************************************/
static
void
globus_l_gfs_iRODS_destroy(
    void *                              user_arg)
{
    globus_l_gfs_iRODS_handle_t *       iRODS_handle;

    iRODS_handle = (globus_l_gfs_iRODS_handle_t *) user_arg;
    globus_mutex_destroy(&iRODS_handle->mutex);
    globus_fifo_destroy(&iRODS_handle->rh_q);
    iRODS_disconnect(iRODS_handle->conn);

    globus_free(iRODS_handle);
}

/*************************************************************************
 *  stat
 *  ----
 *  This interface function is called whenever the server needs 
 *  information about a given file or resource.  It is called then an
 *  LIST is sent by the client, when the server needs to verify that 
 *  a file exists and has the proper permissions, etc.
 ************************************************************************/
static
void
globus_l_gfs_iRODS_stat(
    globus_gfs_operation_t              op,
    globus_gfs_stat_info_t *            stat_info,
    void *                              user_arg)
{
    int                                 status;
    int                                 i;
    globus_gfs_stat_t *                 stat_array;
    globus_gfs_stat_t                   stat_buf;
    int                                 stat_count = 1;
    globus_l_gfs_iRODS_handle_t *         iRODS_handle;
    globus_result_t                     result;
    GlobusGFSName(globus_l_gfs_iRODS_stat);

    iRODS_handle = (globus_l_gfs_iRODS_handle_t *) user_arg;
    /* first test for obvious directories */
    iRODS_l_reduce_path(stat_info->pathname);

    status = iRODS_l_stat1(iRODS_handle->conn, &stat_buf, stat_info->pathname);

    if (status == -808000)
    {
        result = globus_l_gfs_iRODS_make_error("No such file or directory.", status); //UberFTP NEEDS "No such file or directory" in error message
        goto error;
    }
    else if(status < 0)
    {
        result = globus_l_gfs_iRODS_make_error("iRODS_l_stat1 failed.", status);
        goto error;
    } 
    /* iRODSFileStat */
    if(!S_ISDIR(stat_buf.mode) || stat_info->file_only)
    {
        globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "iRODS: globus_l_gfs_iRODS_stat(): single file\n");
        stat_array = (globus_gfs_stat_t *) globus_calloc(
            1, sizeof(globus_gfs_stat_t));
        memcpy(stat_array, &stat_buf, sizeof(globus_gfs_stat_t));
    }
    else
    {
        int rc;
        free(stat_buf.name);
        rc = iRODS_l_stat_dir(iRODS_handle->conn, &stat_array, &stat_count, stat_info->pathname, iRODS_handle->user);

        if(rc != 0)
        {
            result = globus_l_gfs_iRODS_make_error("iRODS_l_stat_dir failed.", rc);
            goto error;
        }
    }

    globus_gridftp_server_finished_stat(
        op, GLOBUS_SUCCESS, stat_array, stat_count);
    /* gota free the names */
    for(i = 0; i < stat_count; i++)
    {
        globus_free(stat_array[i].name);
    }
    globus_free(stat_array);
    return;

error:
    //globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "globus_l_gfs_iRODS_stat(): globus_l_gfs_iRODS_stat Failed. result = %d.\n",result);
    globus_gridftp_server_finished_stat(op, result, NULL, 0);
}

/*************************************************************************
 *  command
 *  -------
 *  This interface function is called when the client sends a 'command'.
 *  commands are such things as mkdir, remdir, delete.  The complete
 *  enumeration is below.
 *
 *  To determine which command is being requested look at:
 *      cmd_info->command
 *
 *      GLOBUS_GFS_CMD_MKD = 1,
 *      GLOBUS_GFS_CMD_RMD,
 *      GLOBUS_GFS_CMD_DELE,
 *      GLOBUS_GFS_CMD_RNTO,
 *      GLOBUS_GFS_CMD_RNFR,
 *      GLOBUS_GFS_CMD_CKSM,
 *      GLOBUS_GFS_CMD_SITE_CHMOD,
 *      GLOBUS_GFS_CMD_SITE_DSI
 ************************************************************************/
static
void
globus_l_gfs_iRODS_command(
    globus_gfs_operation_t              op,
    globus_gfs_command_info_t *         cmd_info,
    void *                              user_arg)
{
    int                                 status = 0;
    globus_l_gfs_iRODS_handle_t *       iRODS_handle;
    char *                              collection;
    globus_result_t                     result = 0;
    char *                              error_str;
    char *                              outChksum = GLOBUS_NULL;
    GlobusGFSName(globus_l_gfs_iRODS_command);

    iRODS_handle = (globus_l_gfs_iRODS_handle_t *) user_arg;

    collection = strdup(cmd_info->pathname);
    iRODS_l_reduce_path(collection);
    if(collection == NULL)
    {
        result = GlobusGFSErrorGeneric("iRODS: strdup failed");
        goto alloc_error;
    }

    switch(cmd_info->command)
    {
        case GLOBUS_GFS_CMD_MKD:
            {
                collInp_t collCreateInp;
                bzero (&collCreateInp, sizeof (collCreateInp));
                rstrcpy (collCreateInp.collName, collection, MAX_NAME_LEN);
                globus_gfs_log_message(GLOBUS_GFS_LOG_INFO,"iRODS: rcCollCreate: collection=%s\n", collection);
                status = rcCollCreate (iRODS_handle->conn, &collCreateInp);
            }
            break;

        case GLOBUS_GFS_CMD_RMD:
            {
                collInp_t rmCollInp;
                bzero (&rmCollInp, sizeof (rmCollInp));
                rstrcpy (rmCollInp.collName, collection, MAX_NAME_LEN);
                addKeyVal (&rmCollInp.condInput, FORCE_FLAG_KW, "");
                globus_gfs_log_message(GLOBUS_GFS_LOG_INFO,"iRODS: rcRmColl: collection=%s\n", collection);
                status = rcRmColl (iRODS_handle->conn, &rmCollInp,0);
            }
            break;

        case GLOBUS_GFS_CMD_DELE:
            {
                dataObjInp_t dataObjInp;
                bzero (&dataObjInp, sizeof (dataObjInp));
                rstrcpy (dataObjInp.objPath, collection, MAX_NAME_LEN);
                addKeyVal (&dataObjInp.condInput, FORCE_FLAG_KW, "");
                globus_gfs_log_message(GLOBUS_GFS_LOG_INFO,"iRODS: rcDataObjUnlink: collection=%s\n", collection);
                status = rcDataObjUnlink(iRODS_handle->conn, &dataObjInp);
            }
            break;

        case GLOBUS_GFS_CMD_CKSM:
           {
               dataObjInp_t dataObjInp;
               bzero (&dataObjInp, sizeof (dataObjInp));
               rstrcpy (dataObjInp.objPath, collection, MAX_NAME_LEN);
               //addKeyVal (&dataObjInp.condInput, FORCE_CHKSUM_KW, "");
               globus_gfs_log_message(GLOBUS_GFS_LOG_INFO,"iRODS: rcDataObjChksum: collection=%s\n", collection);
               status = rcDataObjChksum (iRODS_handle->conn, &dataObjInp, &outChksum);
           }
           break;

       /* case GLOBUS_GFS_CMD_SITE_CHMOD:
            status = 0;
            break;*/

        default:
            break;
    }

    if(status != 0)
    {
        error_str = globus_common_create_string("iRODS error: status = %d", status);
        result = GlobusGFSErrorGeneric(error_str);

        goto error;
    }

    globus_gridftp_server_finished_command(op, GLOBUS_SUCCESS, outChksum);

    free(collection);
    return;

error:
    free(collection);
alloc_error:
    globus_gridftp_server_finished_command(op, result, NULL);
}



/*************************************************************************
 *  recv
 *  ----
 *  This interface function is called when the client requests that a
 *  file be transfered to the server.
 *
 *  To receive a file the following functions will be used in roughly
 *  the presented order.  They are doced in more detail with the
 *  gridftp server documentation.
 *
 *      globus_gridftp_server_begin_transfer();
 *      globus_gridftp_server_register_read();
 *      globus_gridftp_server_finished_transfer();
:w *
 ************************************************************************/
static
void
globus_l_gfs_iRODS_recv(
    globus_gfs_operation_t              op,
    globus_gfs_transfer_info_t *        transfer_info,
    void *                              user_arg)
{
    globus_l_gfs_iRODS_handle_t *       iRODS_handle;
    int                                 flags = O_WRONLY;
    globus_bool_t                       finish = GLOBUS_FALSE;
    char *                              collection = NULL;
    dataObjInp_t                        dataObjInp;
    openedDataObjInp_t                  dataObjWriteInp;
    int result; 

    GlobusGFSName(globus_l_gfs_iRODS_recv);
    iRODS_handle = (globus_l_gfs_iRODS_handle_t *) user_arg;
    //Get iRODS resource from destination path
    if(iRODS_Resource_struct.resource != NULL && iRODS_Resource_struct.path != NULL)
    {
        if(strncmp(iRODS_Resource_struct.path, transfer_info->pathname, strlen(iRODS_Resource_struct.path)) != 0 )
        {
            iRODS_getResource(transfer_info->pathname);
        }
    }
    else
    {
        iRODS_getResource(transfer_info->pathname);
    }

    if(iRODS_handle == NULL)
    {
        /* dont want to allow clear text so error out here */
        result = GlobusGFSErrorGeneric("iRODS DSI must be a default backend"
            " module.  It cannot be an eret alone");
        goto alloc_error;
    }   

    if(transfer_info->truncate)
    {
        flags |= O_TRUNC;
    }

    iRODS_l_reduce_path(transfer_info->pathname);
    collection = strdup(transfer_info->pathname);
    if(collection == NULL)
    {
        result = GlobusGFSErrorGeneric("iRODS: strdup failed");
        goto alloc_error;
    }

    //create the obj
    bzero (&dataObjInp, sizeof (dataObjInp));
    bzero (&dataObjWriteInp, sizeof (dataObjWriteInp));
    rstrcpy (dataObjInp.objPath, collection, MAX_NAME_LEN);
    dataObjInp.dataSize = 0;
    addKeyVal (&dataObjInp.condInput, FORCE_FLAG_KW, "");
    if (iRODS_Resource_struct.resource != NULL)
    {
        addKeyVal (&dataObjInp.condInput, DEST_RESC_NAME_KW, iRODS_Resource_struct.resource);
        globus_gfs_log_message(GLOBUS_GFS_LOG_INFO,"iRODS: Creating file with resource: %s\n", iRODS_Resource_struct.resource);
    }
    iRODS_handle->fd = rcDataObjCreate (iRODS_handle->conn, &dataObjInp);
    if (iRODS_handle->fd < 0) {
        result = globus_l_gfs_iRODS_make_error("rcDataObjCreate failed", iRODS_handle->fd);
        goto error;
    }  
    globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "iRODS: Creating file succeeded. File created: %s.\n", collection);
    free(collection);


    iRODS_handle = (globus_l_gfs_iRODS_handle_t *) user_arg;
    /* reset all the needed variables in the handle */
    iRODS_handle->cached_res = GLOBUS_SUCCESS;
    iRODS_handle->outstanding = 0;
    iRODS_handle->done = GLOBUS_FALSE;
    iRODS_handle->blk_length = 0;
    iRODS_handle->blk_offset = 0;
    iRODS_handle->op = op;
    globus_gridftp_server_get_block_size(
        op, &iRODS_handle->block_size);


    globus_gridftp_server_begin_transfer(op, 0, iRODS_handle);

    globus_mutex_lock(&iRODS_handle->mutex);
    {
        finish = globus_l_gfs_iRODS_read_from_net(iRODS_handle);
    }
    globus_mutex_unlock(&iRODS_handle->mutex);
   
    if(finish)
    {
        globus_gridftp_server_finished_transfer(iRODS_handle->op, iRODS_handle->cached_res);
    }

    return;

error:
alloc_error:
    globus_gridftp_server_finished_transfer(op, result);


}

/*************************************************************************
 *  send
 *  ----
 *  This interface function is called when the client requests to receive
 *  a file from the server.
 *
 *  To send a file to the client the following functions will be used in roughly
 *  the presented order.  They are doced in more detail with the
 *  gridftp server documentation.
 *
 *      globus_gridftp_server_begin_transfer();
 *      globus_gridftp_server_register_write();
 *      globus_gridftp_server_finished_transfer();
 *
 ************************************************************************/
static
void
globus_l_gfs_iRODS_send(
    globus_gfs_operation_t              op,
    globus_gfs_transfer_info_t *        transfer_info,
    void *                              user_arg)
{
    globus_bool_t                       done = GLOBUS_FALSE;
    globus_bool_t                       finish = GLOBUS_FALSE;    
    globus_l_gfs_iRODS_handle_t *       iRODS_handle;
    globus_result_t                     result;
    char *                              collection;
    dataObjInp_t                        dataObjInp;    
    int                                 i = 0;

    GlobusGFSName(globus_l_gfs_iRODS_send);

    iRODS_handle = (globus_l_gfs_iRODS_handle_t *) user_arg;
    if(iRODS_handle == NULL)
    {
        /* dont want to allow clear text so error out here */
        result = GlobusGFSErrorGeneric("iRODS DSI must be a default backend module. It cannot be an eret alone");
        goto alloc_error;
    }

    collection = strdup(transfer_info->pathname);
    if(collection == NULL)
    {
        result = GlobusGFSErrorGeneric("iRODS: strdup failed");
        goto alloc_error;
    }
    iRODS_l_reduce_path(collection);
    
    bzero (&dataObjInp, sizeof (dataObjInp));
    rstrcpy (dataObjInp.objPath, collection, MAX_NAME_LEN);
    iRODS_handle->fd = rcDataObjOpen (iRODS_handle->conn, &dataObjInp);
    
    if (iRODS_handle->fd < 0) {
        result = globus_l_gfs_iRODS_make_error("rcDataObjOpen failed.", iRODS_handle->fd);
        goto error;
    }  
    globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "iRODS: rcDataObjOpen: %s\n", collection);

    /* reset all the needed variables in the handle */
    iRODS_handle->read_eof = GLOBUS_FALSE;
    iRODS_handle->cached_res = GLOBUS_SUCCESS;
    iRODS_handle->outstanding = 0;
    iRODS_handle->done = GLOBUS_FALSE;
    iRODS_handle->blk_length = 0;
    iRODS_handle->blk_offset = 0;
    iRODS_handle->op = op;
    globus_gridftp_server_get_optimal_concurrency(
        op, &iRODS_handle->optimal_count);
    globus_gridftp_server_get_block_size(
        op, &iRODS_handle->block_size);

    globus_gridftp_server_begin_transfer(op, 0, iRODS_handle);
   
    globus_mutex_lock(&iRODS_handle->mutex);
    {

        for(i = 0; i < iRODS_handle->optimal_count && !done; i++)
        {
            globus_l_gfs_iRODS_read_ahead_next(iRODS_handle);
            done = globus_l_gfs_iRODS_send_next_to_client(iRODS_handle);
        }
        for(i = 0; i < iRODS_handle->optimal_count && !done; i++)
        {
            globus_l_gfs_iRODS_read_ahead_next(iRODS_handle);
        }
        if(done && iRODS_handle->outstanding == 0 &&
            globus_fifo_empty(&iRODS_handle->rh_q))
        {
            finish = GLOBUS_TRUE;
        }
    }
    globus_mutex_unlock(&iRODS_handle->mutex);
    if(finish)
    {
        globus_gridftp_server_finished_transfer(op, iRODS_handle->cached_res);
    }

    globus_free(collection);

    return;

error:
    globus_free(collection);
alloc_error:
    globus_gridftp_server_finished_transfer(op, result);
}

/*************************************************************************
 *         logic to receive from client
 *         ----------------------------
 ************************************************************************/

static
void
globus_l_gfs_iRODS_net_read_cb(
    globus_gfs_operation_t              op,
    globus_result_t                     result,
    globus_byte_t *                     buffer,
    globus_size_t                       nbytes,
    globus_off_t                        offset,
    globus_bool_t                       eof,
    void *                              user_arg)
{
    globus_bool_t                       finished = GLOBUS_FALSE;
    globus_l_gfs_iRODS_handle_t *       iRODS_handle;
    globus_size_t                       bytes_written;

    iRODS_handle = (globus_l_gfs_iRODS_handle_t *) user_arg;

    globus_mutex_lock(&iRODS_handle->mutex);
    {
        if(eof)
        {
            iRODS_handle->done = GLOBUS_TRUE;
        }
        iRODS_handle->outstanding--;
        if(result != GLOBUS_SUCCESS)
        {
            iRODS_handle->cached_res = result;
            iRODS_handle->done = GLOBUS_TRUE;
        }
        /* if the read was successful write to disk */
        else if(nbytes > 0)
        {
            openedDataObjInp_t dataObjLseekInp;
            bzero (&dataObjLseekInp, sizeof (dataObjLseekInp));
            dataObjLseekInp.l1descInx = iRODS_handle->fd;
            fileLseekOut_t *dataObjLseekOut = NULL;
            dataObjLseekInp.offset = offset;
            dataObjLseekInp.whence = SEEK_SET;

            int status = rcDataObjLseek(iRODS_handle->conn, &dataObjLseekInp, &dataObjLseekOut);
	    // verify that it worked
            if(status < 0)
            {
                iRODS_handle->cached_res = globus_l_gfs_iRODS_make_error("rcDataObjLseek failed", status);
                iRODS_handle->done = GLOBUS_TRUE;
            }
            else
            {
               openedDataObjInp_t dataObjWriteInp;
               bzero (&dataObjWriteInp, sizeof (dataObjWriteInp));
               dataObjWriteInp.l1descInx = iRODS_handle->fd;
               dataObjWriteInp.len = nbytes;

               bytesBuf_t dataObjWriteInpBBuf;
               dataObjWriteInpBBuf.buf = buffer;
               dataObjWriteInpBBuf.len = nbytes;

               bytes_written  = rcDataObjWrite(iRODS_handle->conn, &dataObjWriteInp, &dataObjWriteInpBBuf); //buffer need to be casted??
               if (bytes_written < nbytes) {
                   iRODS_handle->cached_res = globus_l_gfs_iRODS_make_error("rcDataObjWrite failed", bytes_written);
                   iRODS_handle->done = GLOBUS_TRUE;
               }
               globus_gridftp_server_update_bytes_written(op, offset, bytes_written);
            }
        }


        globus_free(buffer);
        /* if not done just register the next one */
        if(!iRODS_handle->done)
        {
            finished = globus_l_gfs_iRODS_read_from_net(iRODS_handle);
        }
        /* if done and there are no outstanding callbacks finish */
        else if(iRODS_handle->outstanding == 0)
        {
            openedDataObjInp_t dataObjCloseInp;
            bzero (&dataObjCloseInp, sizeof (dataObjCloseInp));
            dataObjCloseInp.l1descInx = iRODS_handle->fd;
            rcDataObjClose(iRODS_handle->conn, &dataObjCloseInp);
            finished = GLOBUS_TRUE;
        }
    }
    globus_mutex_unlock(&iRODS_handle->mutex);

    if(finished)
    {
        globus_gridftp_server_finished_transfer(op, iRODS_handle->cached_res);
    }
}


static
globus_bool_t
globus_l_gfs_iRODS_read_from_net(
    globus_l_gfs_iRODS_handle_t *         iRODS_handle)
{
    globus_byte_t *                     buffer;
    globus_result_t                     result;
    GlobusGFSName(globus_l_gfs_iRODS_read_from_net);


    /* in the read case tis number will vary */
    globus_gridftp_server_get_optimal_concurrency(
        iRODS_handle->op, &iRODS_handle->optimal_count);


    while(iRODS_handle->outstanding < iRODS_handle->optimal_count)
    {
        buffer = globus_malloc(iRODS_handle->block_size);
        if(buffer == NULL)
        {
            result = GlobusGFSErrorGeneric("malloc failed");
            goto error;
        }
        result = globus_gridftp_server_register_read(
            iRODS_handle->op,
            buffer,
            iRODS_handle->block_size,
            globus_l_gfs_iRODS_net_read_cb,
            iRODS_handle);
        if(result != GLOBUS_SUCCESS)
        {
            goto alloc_error;
        }
        iRODS_handle->outstanding++;
    }

    return GLOBUS_FALSE;

alloc_error:
    globus_free(buffer);
error:
    iRODS_handle->cached_res = result;
    iRODS_handle->done = GLOBUS_TRUE;
    if(iRODS_handle->outstanding == 0)
    {
        openedDataObjInp_t dataObjCloseInp;
        bzero (&dataObjCloseInp, sizeof (dataObjCloseInp));
        dataObjCloseInp.l1descInx = iRODS_handle->fd;
        rcDataObjClose(iRODS_handle->conn, &dataObjCloseInp);
        return GLOBUS_TRUE;
    }
    return GLOBUS_FALSE;
}


/*************************************************************************
 *         logic for sending to the client
 *         ----------------------------
 ************************************************************************/
static
void
globus_l_gfs_net_write_cb(
    globus_gfs_operation_t              op,
    globus_result_t                     result,
    globus_byte_t *                     buffer,
    globus_size_t                       nbytes,
    void *                              user_arg)
{
    globus_bool_t                       finish = GLOBUS_FALSE;
    globus_l_gfs_iRODS_handle_t *         iRODS_handle;
    globus_l_iRODS_read_ahead_t *         rh;
    globus_l_iRODS_read_ahead_t *         tmp_rh;

    rh = (globus_l_iRODS_read_ahead_t *) user_arg;
    iRODS_handle = rh->iRODS_handle;
    globus_free(rh);

    globus_mutex_lock(&iRODS_handle->mutex);
    {
        iRODS_handle->outstanding--;
        if(result != GLOBUS_SUCCESS)
        {
            iRODS_handle->cached_res = result;
            iRODS_handle->read_eof = GLOBUS_TRUE;
            openedDataObjInp_t dataObjCloseInp;
            bzero (&dataObjCloseInp, sizeof (dataObjCloseInp));
            dataObjCloseInp.l1descInx = iRODS_handle->fd;
            rcDataObjClose(iRODS_handle->conn, &dataObjCloseInp);
            while(!globus_fifo_empty(&iRODS_handle->rh_q))
            {
                tmp_rh = (globus_l_iRODS_read_ahead_t *)
                    globus_fifo_dequeue(&iRODS_handle->rh_q);
                globus_free(tmp_rh);
            }
        }
        else
        {
            globus_l_gfs_iRODS_send_next_to_client(iRODS_handle);
            globus_l_gfs_iRODS_read_ahead_next(iRODS_handle);
        }
        /* if done and there are no outstanding callbacks finish */
        if(iRODS_handle->outstanding == 0 &&
            globus_fifo_empty(&iRODS_handle->rh_q))
        {
            finish = GLOBUS_TRUE;
        }
    }
    globus_mutex_unlock(&iRODS_handle->mutex);

    if(finish)
    {
        globus_gridftp_server_finished_transfer(op, iRODS_handle->cached_res);
    }
}


static
globus_bool_t
globus_l_gfs_iRODS_send_next_to_client(
    globus_l_gfs_iRODS_handle_t *         iRODS_handle)
{
    globus_l_iRODS_read_ahead_t *         rh;
    globus_result_t                     res;
    GlobusGFSName(globus_l_gfs_iRODS_send_next_to_client);
    
    rh = (globus_l_iRODS_read_ahead_t *) globus_fifo_dequeue(&iRODS_handle->rh_q);
    if(rh == NULL)
    {
        goto error;
    }

    res = globus_gridftp_server_register_write(
        iRODS_handle->op, rh->buffer, rh->length, rh->offset, -1, 
        globus_l_gfs_net_write_cb, rh);
    if(res != GLOBUS_SUCCESS)
    {
        goto alloc_error;
    }
    iRODS_handle->outstanding++;
    return GLOBUS_FALSE;

alloc_error:
    globus_free(rh);

    iRODS_handle->cached_res = res;
    if(!iRODS_handle->read_eof)
    {
        openedDataObjInp_t dataObjCloseInp;
        bzero (&dataObjCloseInp, sizeof (dataObjCloseInp));
        dataObjCloseInp.l1descInx = iRODS_handle->fd;
        rcDataObjClose(iRODS_handle->conn, &dataObjCloseInp);
        iRODS_handle->read_eof = GLOBUS_TRUE;
    }
    /* if we get an error here we need to flush the q */
    while(!globus_fifo_empty(&iRODS_handle->rh_q))
    {
        rh = (globus_l_iRODS_read_ahead_t *)
            globus_fifo_dequeue(&iRODS_handle->rh_q);
        globus_free(rh);
    }

error:
    return GLOBUS_TRUE;
}



static
void
globus_l_gfs_iRODS_read_ahead_next(
    globus_l_gfs_iRODS_handle_t *         iRODS_handle)
{
    int                                 read_length;
    globus_result_t                     result;
    globus_l_iRODS_read_ahead_t *         rh;
    GlobusGFSName(globus_l_gfs_iRODS_read_ahead_next);   
    if(iRODS_handle->read_eof)
    {
        goto error;
    }
    /* if we have done everything for this block, get the next block
       also this will happen the first time
       -1 length means until the end of the file  */

    if(iRODS_handle->blk_length == 0)
    {
        /* check the next range to read */
        globus_gridftp_server_get_read_range(
            iRODS_handle->op,
            &iRODS_handle->blk_offset,
            &iRODS_handle->blk_length);
        if(iRODS_handle->blk_length == 0)
        {
            result = GLOBUS_SUCCESS;
            goto error;
        }
    }

    /* get the current length to read */
    if(iRODS_handle->blk_length == -1 || iRODS_handle->blk_length > iRODS_handle->block_size)
    {
        read_length = (int)iRODS_handle->block_size;
    }
    else
    {
        read_length = (int)iRODS_handle->blk_length;
    }
    rh = (globus_l_iRODS_read_ahead_t *) calloc(1,
        sizeof(globus_l_iRODS_read_ahead_t)+read_length);
    rh->offset = iRODS_handle->blk_offset;
    rh->iRODS_handle = iRODS_handle;

    openedDataObjInp_t dataObjLseekInp;
    bzero (&dataObjLseekInp, sizeof (dataObjLseekInp));
    dataObjLseekInp.l1descInx = iRODS_handle->fd;
    fileLseekOut_t *dataObjLseekOut = NULL;
    dataObjLseekInp.offset = (long)iRODS_handle->blk_offset;
    dataObjLseekInp.whence = SEEK_SET;

    int status = rcDataObjLseek(iRODS_handle->conn, &dataObjLseekInp, &dataObjLseekOut);   
    // verify that it worked
    if(status < 0)
    {
        result = globus_l_gfs_iRODS_make_error("rcDataObjLseek failed", status);
        goto attempt_error;
    }

    openedDataObjInp_t dataObjReadInp;
    bzero (&dataObjReadInp, sizeof (dataObjReadInp));
    dataObjReadInp.l1descInx = iRODS_handle->fd;
    dataObjReadInp.len = read_length;

    bytesBuf_t dataObjReadOutBBuf;
    bzero (&dataObjReadOutBBuf, sizeof (dataObjReadOutBBuf));
    
    //bytesBuf_t DataObjReadOutBBuf;
    dataObjReadOutBBuf.buf = rh->buffer;
    dataObjReadOutBBuf.len = read_length;//rh->length;

    rh->length = rcDataObjRead (iRODS_handle->conn, &dataObjReadInp, &dataObjReadOutBBuf);
    if(rh->length <= 0)
    {
        result = GLOBUS_SUCCESS; /* this may just be eof */
        goto attempt_error;
    }

    iRODS_handle->blk_offset += rh->length;
    if(iRODS_handle->blk_length != -1)
    {
        iRODS_handle->blk_length -= rh->length;
    }

    globus_fifo_enqueue(&iRODS_handle->rh_q, rh);
    return;

attempt_error:
    globus_free(rh);
    openedDataObjInp_t dataObjCloseInp;
    bzero (&dataObjCloseInp, sizeof (dataObjCloseInp));
    dataObjCloseInp.l1descInx = iRODS_handle->fd;
    rcDataObjClose(iRODS_handle->conn, &dataObjCloseInp);
    iRODS_handle->cached_res = result;
    
error:
    iRODS_handle->read_eof = GLOBUS_TRUE;
}


static
int
globus_l_gfs_iRODS_activate(void);

static
int
globus_l_gfs_iRODS_deactivate(void);

/*
 *  no need to change this
 */
static globus_gfs_storage_iface_t       globus_l_gfs_iRODS_dsi_iface = 
{
    0,//GLOBUS_GFS_DSI_DESCRIPTOR_BLOCKING | GLOBUS_GFS_DSI_DESCRIPTOR_SENDER,
    globus_l_gfs_iRODS_start,
    globus_l_gfs_iRODS_destroy,
    NULL, /* list */
    globus_l_gfs_iRODS_send,
    globus_l_gfs_iRODS_recv,
    NULL, /* trev */
    NULL, /* active */
    NULL, /* passive */
    NULL, /* data destroy */
    globus_l_gfs_iRODS_command, 
    globus_l_gfs_iRODS_stat,
    NULL,
    NULL
};

/*
 *  no need to change this
 */
GlobusExtensionDefineModule(globus_gridftp_server_iRODS) =
{
    "globus_gridftp_server_iRODS",
    globus_l_gfs_iRODS_activate,
    globus_l_gfs_iRODS_deactivate,
    NULL,
    NULL,
    &local_version
};

/*
 *  no need to change this
 */
static
int
globus_l_gfs_iRODS_activate(void)
{
    globus_extension_registry_add(
        GLOBUS_GFS_DSI_REGISTRY,
        "iRODS",
        GlobusExtensionMyModule(globus_gridftp_server_iRODS),
        &globus_l_gfs_iRODS_dsi_iface);
    
    return 0;
}

/*
 *  no need to change this
 */
static
int
globus_l_gfs_iRODS_deactivate(void)
{
    globus_extension_registry_remove(
        GLOBUS_GFS_DSI_REGISTRY, "iRODS");

    return 0;
}
