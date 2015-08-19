B2STAGE-GridFTP (iRODS-DSI)
===============

B2STAGE service core code for EUDAT project: DSI interface


The GridFTP server provides high speed remote access to data stores. 
There are many different types of data storage systems from standard 
file systems to arrays of magnetic tape. To allow GridFTP to be used 
with as many data storage systems as possible, the GridFTP can be 
extended, implementing an interface called Data Storage Interface (DSI).

The GridFTP iRODS DSI consists of C based functions, which, through 
the iRODS C API, can interact with iRODS. The main supported operations 
are get, put, delete and list.

The module can be loaded by the GridFTP server at start-up time through 
a specific command line option, therefore no changes are required in the
GridFTP server typical configuration, which makes easier the maintenance 
of the module being decoupled from future changes of the server.




Building iRODS DSI with CMake
--------------------------------

Prerequisite: 

- CMake 2.7 or higher

- globus-gridftp-server-progs
- libglobus-common-dev (.deb) or globus-common-devel (.rpm)
- libglobus-gridftp-server-dev (.deb) or globus-gridftp-server-devel (.rpm)
- libglobus-gridmap-callout-error-dev (.deb) or globus-gridmap-callout-error-devel (.rpm)
(see http://www.ige-project.eu/downloads/software/releases/downloads)

The installation is quite simple: it is possible to use the official gridftp 
server package without recompiling it.


1. Create a deployment folder:

        mkdir /preferred_path/iRODS_DSI
    
2. Set the following environment variables (it can be done editing and renaming 
   the "setup.sh.template" file in "setup.sh" and source it):

   - GLOBUS_LOCATION --> path to the Globus installation (if you have installed from packages, use '/usr')
   - IRODS_PATH --> path to the iRODS installation (if you have installed from packages, use '/usr')
   - FLAVOR --> (optional, usually not needed) flavors of the Globus packages which are      already installed [a] 
   - DEST_LIB_DIR -->  ````/preferred_path/iRODS_DSI````
   - DEST_BIN_DIR --> ````/preferred_path/iRODS_DSI````
   - DEST_ETC_DIR -->  ````/preferred_path/iRODS_DSI````
   - RESOURCE_MAP_PATH --> (optional) path to the folder containing the "irodsResourceMap.conf" file (see step 4 of section "Configure and run") 
   - IRODS_40_COMPAT --> (optional) Use iRODS 4.0.x compatible library file locations.  (Use for iRODS 4.0.x only, not needed for iRODS 4.1.+).

   Note: comment out any variables not set (as leaving them set to a blank
   value would prevent CMake from constructing correct default values for
   variables that are optional).

   [a] This depends on your globus installation. You will probably not need it. 
       In case of error, possible flavors are:
       
            FLAVOR=gcc64dbg
            
       or
       
            FLAVOR=gcc64dbgpthr
            
       You can access this information with gpt-query. 
       Anyway, if the flavor is not correct, the error returned when attempting to run the server should help.

3. Run cmake:
   
        $ cmake (path to CMakeFile.txt)

4. Compile the module:

        $ make

   An error like:
   
        ${IRODS_PATH}/lib/core/obj/libRodsAPIs.a(clientLogin.o):
        relocation R_X86_64_32 against `.bss` can not be used when making a
        shared object; recompile with -fPIC

   usually happens on x86_64 systems. In order to solve it, recompile iRODS with 
   the mentioned flag, -fPIC:
   
   a) Edit iRODS/config/irods.config and add:
   
       $CCFLAGS = '-fPIC';
   
   b) Rebuild iRODS with ````./irodssetup````
  
5. ````$ make install````
        
   Alternatively you can add the path to the library libglobus_gridftp_server_iRODS.so
   to the env variable $LD_LIBRARY_PATH in the same environment where you start the 
   GridFTP server.



Configuring and run
--------------------------------

In order to run the server as an unprivileged user (root privileges are not 
required):

1. Create */preferred_path/iRODS_DSI/irods_environment.json* or */preferred_path/iRODS_DSI/.irodsEnv* (for iRODS < 4.1.x) populating at least *irods_host*, *irods_port*, *irods_zone_name* and also *irods_user_name* and *irods_default_resource*):

        {
          "irods_host" : "irods4",
           "irods_port" : 1247,
           "irods_user_name" : "rods",
           "irods_zone_name" : "tempZone",
           "irods_default_resource" : "demoResc"
        }
          
2. Add the following to /etc/gridftp.conf (or a file in /etc/gridftp.d such as /etc/gridftp.d/irods):

        $IRODS_ENVIRONMENT_FILE "/preferred_path/iRODS_DSI/irods_environment.json"
        #$irodsEnvFile "/preferred_path/iRODS_DSI/.irodsEnv"  #for iRODS < 4.1.x
        $LD_LIBRARY_PATH "$LD_LIBRARY_PATH:/home/ubuntu/DSI/iRODS_DSI"
        $irodsConnectAsAdmin "rods" #irods admin username
        load_dsi_module iRODS 
        auth_level 4

3. Make sure the GridFTP server certificate DN is associated with the irods admin account specified in the previous set $irodsConnectAsAdmin variable, for example:

        $ iadmin lua | grep rods
         rods /C=US/O=Globus Consortium/OU=Globus Connect Service/CN=host/develvm01.pico.cineca.it


4. When deploying with iRODS 4, it is necessary to preload the DSI library into
   the GridFTP server binary, so that the symbols exported by the library are
   visible.  Otherwise, when iRODS 4 tries loading plugins (including basic
   network and authentication plugins), the plugins would fail to load (as the
   plugins are not explicitly declaring a dependency on the runtime symbols).

   To preload the library, you need to set the LD_PRELOAD variable to
   ````$DEST_LIB_DIR/libglobus_gridftp_server_iRODS.so```` (or
   ````$DEST_LIB_DIR/libglobus_gridftp_server_iRODS_$FLAVOR.so````) in the
   environment from which the GridFTP server is started (i.e., it is not enough
   to set it in /etc/gridftp.conf).

   For example, modify (or create) ````/etc/sysconfig/globus-gridftp-server````
   and add the lines:

        LD_PRELOAD="$LD_PRELOAD:/usr/lib64/libglobus_gridftp_server.so:/opt/iRODS_DSI/iRODS_DSI/libglobus_gridftp_server_iRODS.so"
        export LD_PRELOAD

   Note: it is necessary to load the GridFTP server library alongside the DSI
   library (which depends on symbols provided by the GridFTP server library).
   Otherwise, any command invocation in that environment fails with unresolved
   symbol errors.

   Also, in order for GridFTP CKSM (checksum) command to work (and interoperate
   with Globus.org), it is necessary to configure iRODS to use MD5 checksums
   (iRODS 4 otherwise defaults to SHA-256).  Edit ````/etc/irods/server.config````
   and set:

        default_hash_scheme MD5

5. Modify the globus-gridftp-server script in /etc/init.d in order to create log
   and pid files where the user who is running the server has the required 
   ownership and run the server with:

        $ export LD_LIBRARY_PATH=/preferred_path/iRODS_DSI/
        $ /etc/init.d/globus-gridftp-server start
   


Additional configuration
--------------------------------
1. If desired, change the default home directory by setting the homeDirPattern
   environment variable in ````/etc/gridftp.conf````.  The pattern can
   reference up to two strings with ````%s````, first gets substituted with the
   zone name, second with the user name.  The default value is
   ````"/%s/home/%s"````, making the default directory ````/<zone>/home/<username>````.

   Default configuration:

        $homeDirPattern "/%s/home/%s"

   Example alternative configuration (defaulting to ````/<zone>/home````):

        $homeDirPattern "/%s/home"
        
2. It is possible to specify a policy to manage more than one iRODS resource.
   The DSI module looks for a file called 'irodsresourcemap.conf' (step 1 of the
   Installation paragraph). In that file it is possible to specify which iRODS 
   resource has to be used when creating a file in a particular iRODS path.
   For example:
   
        $ cat irodsResourceMap.conf 
        /CINECA01/home/cin_staff/rmucci00;resc-repl
        /CINECA01/home/cin_staff/mrossi;resc-repl

   If none of the listed paths is matched, the iRODS default resource is used. 



4. Optionally, use a Globus gridmap callout module to map subject DNs to iRODS user names based on the existing mappings in iRODS (in r_user_auth table). Configuring this feature eliminates the need for a local grid map file - all user mappings can be done through the callout function.

    The gridmap callout configuration file gets already created as '$DEST_ETC_DIR/gridmap_iRODS_callout.conf'.

    To activate the module, set the '$GSI_AUTHZ_CONF' environment variable in '/etc/gridftp.conf' to point to the configuration file - already created as '$DEST_ETC_DIR/gridmap_iRODS_callout.conf'.

        $GSI_AUTHZ_CONF /etc/grid-security/gridmap_iRODS_callout.conf
        
    Note that in order for this module to work, the server certificate DN must be authorized to connect as a rodsAdmin user (e.g., the 'rods' user).

    This module also supports invoking an iRODS server-side command with iexec in case the DN does not have a mapping yet. The command would receive the DN being mapped as a single argument and may for example add a mapping to an existing account, or create a new account.

    To enable this feature, set the '$irodsDnCommand' environment variable in '/etc/gridftp.conf' to the name of the command to execute. On the iRODS server, the command should be installed in '$IRODS_HOME/server/bin/cmd/'. For example, to invoke a script called 'createUser', add:

        $irodsDnCommand "createUser"
    
    There is also a command line utility to test the mapping lookups (and script execution) that would otherwise be done by the gridmap module. This utility command gets installed into '$DEST_BIN_DIR/testirodsmap' and should be invoked with the DN as a single argument. The command would need to see the same environment variables as the gridmap module loaded into the GridFTP server - specifically, '$irodsEnvFile' pointing to the iRODS environment and '$irodsDnCommand' setting the command to invoke if no mapping is found. The 'testirodsmap' command also needs to have access to the server host certificate - and find it either through the default mechanisms used by Globus GSI or by explicitly setting the 'X509_USER_CERT' and 'X509_USER_KEY' environment variables.
(The easiest way is to run the command in the same environment as the Globus GridFTP server, i.e., under the root account). For example, invoke the command with:

        export irodsDnCommand=createUser 
        export irodsEnvFile=/path/to/.irodsEnv
        $DEST_BIN_DIR/testirodsmap "/C=XX/O=YYY/CN=Example User"

4. Optionally (not recommend), turn off the feature that would make the DSI module authenticate as the rods admin user - but operate under the privileges of the target user.

    To do so, remove the line

        $irodsConnectAsAdmin "rods"

    from the /etc/gridftp.conf file.

    It is than necessary to associate the DN of the server with each individual user.
    In the iCAT associate the irods usernames to the user DNs as well as to the gridftp server DN, for example:

        $ iadmin lua | grep mrossi
        mrossi /C=IT/O=INFN/OU=Personal Certificate/L=CINECA-SAP/CN=Mario Rossi
        mrossi /C=IT/O=INFN/OU=Host/L=CINECA-SAP/CN=fec03.cineca.it

   
    NOTE: this feature is necessary using an  iRODS server version earlier than 3.3.



Logrotate
--------------------------------
If you use -d ALL as in the example, please, be aware that the log files could 
grow quite a lot so the use of logrotate is suggested. 




Licence
---------------------------------
 Copyright (c) 2013 CINECA (www.hpc.cineca.it)
 
 Copyright (c) 1999-2006 University of Chicago
  
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
  
 http://www.apache.org/licenses/LICENSE-2.0
 
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 
 
 Globus DSI to manage data on iRODS.
 
 Author: Roberto Mucci - SCAI - CINECA
 Email:  hpc-service@cineca.it

 Globus gridmap iRODS callout to map subject DNs to iRODS accounts.
 
 Author: Vladimir Mencl, University of Canterbury
 Email:  vladimir.mencl@canterbury.ac.nz

