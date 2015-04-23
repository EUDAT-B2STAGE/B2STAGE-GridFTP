B2STAGE-GridFTP
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

- CMake 2.8 or higher

- globus-gridftp-server-progs
- libglobus-common-dev (.deb) or globus-common-devel (.rpm)
- libglobus-gridftp-server-dev (.deb) or globus-gridftp-server-devel (.rpm)
- libglobus-gridmap-callout-error-dev (.deb) or globus-gridmap-callout-error-devel (.rpm)
(see http://www.ige-project.eu/downloads/software/releases/downloads)


Installation
--------------------------------

The installation is quite simple: it is possible to use the official gridftp 
server package without recompiling it.

1) Set the following environment variables (it can be done editing and renaming 
   the "setup.sh.template" file):

   - GLOBUS_LOCATION --> path to the Globus installation (if you have installed
     the Globus GridFTP Server from packages, use '/usr')
   - IRODS_PATH --> path to the iRODS installation
   - FLAVOR --> (optional) flavors of the Globus packages which are already installed[a] 
   - DEST_LIB_DIR --> (optional) path to the folder in which the library will be copied
   - DEST_BIN_DIR --> (optional) path to the folder in which binary executables (auxilliary tools) will be copied
   - DEST_ETC_DIR --> (optional) path to the folder in which configuration files will be copied
   - RESOURCE_MAP_PATH --> (optional) path to the folder containing the 
     "irodsResourceMap.conf" file (see step 4 of section "Configure and run") 

[a] This depends on your globus installation. You will probably not need it. 
   In case of error, possible flavors are:
   FLAVOR=gcc64dbg
   or
   FLAVOR=gcc64dbgpthr
   You can access this information with gpt-query. 
   Anyway, if the flavor is not correct, the error running the server should help.

2) Run cmake:
   
   $ cmake (path to CMakeFile.txt)

3) Compile the module:

   $ make

   An error like:
   ${IRODS_PATH}/lib/core/obj/libRodsAPIs.a(clientLogin.o):
   relocation R_X86_64_32 against `.bss` can not be used when making a
   shared object; recompile with -fPIC

   usually happens on x86_64 systems. In order to solve it, recompile iRODS with 
   the mentioned flag, -fPIC. This can be done in three alternative ways:

   a) in ${IRODS_PATH} modify
      * clients/icommands/Makefile
      * lib/Makefile
      * server/Makefile
      adding the following line:
      CFLAGS +=  -fPIC
      * make clean && make

   b) in ${IRODS_PATH} modify CCFLAGS variable:  
      * config/irods.config:
        $CCFLAGS = '-fPIC'; 
      * config/platform.mk:
        CCFLAGS=-fPIC
      * irodssetup

   c) in ${IRODS_PATH}:
      * export CFLAGS="$CFLAGS -fPIC"
      * make clean && make

   The solution 'a' has the advantage of woorking even if you later recompile 
   iRODS again. The solution 'b' is the same as solution 'a', but using
   irodssetup instead of make.  That makes the changes persist even after
   irodssetup is re-run. The solution 'c' is faster to be implemented. 

4) Install the module into the GLOBUS_LOCATION. To do this you will need write 
   permission for that directory:

   $ make install 

   If you have installed the Globus GridFTP Server from packages:

   $ sudo make install
  


Configure and run
--------------------------------

In order to run the server as an unprivileged user (root privileges are not 
required):

1) Modify '/etc/gridftp.conf' adding:

      $GRIDMAP "/path/to/grid-mapfile"  
      $irodsEnvFile "/path/to/.irodsEnv" 
      load_dsi_module iRODS 
      auth_level 4

   where '/path/to/.irodsEnv' contains at least the lines:

       irodsHost 'your.irods.server'
       irodsPort 'port'
       irodsZone 'zone'
       irodsAuthScheme 'GSI'


2) In /etc/grid-security/grid-mapfile associate the DNs to the irods usernames, 
   for example:

   $ grep mrossi /etc/grid-security/grid-mapfile
   "/C=IT/O=INFN/OU=Personal Certificate/L=CINECA-SAP/CN=Mario Rossi" mrossi

3) In the iCAT associate the irods usernames to the user DNs as well as to 
   the gridftp server DN, for example:

   $ iadmin lua | grep mrossi
   mrossi /C=IT/O=INFN/OU=Personal Certificate/L=CINECA-SAP/CN=Mario Rossi
   mrossi /C=IT/O=INFN/OU=Host/L=CINECA-SAP/CN=fec03.cineca.it

4) It is possible to specify a policy to manage more than one iRODS resource.
   The DSI module looks for a file called 'irodsresourcemap.conf' (step 1 of the
   Installation paragraph). In that file it is possible to specify which iRODS 
   resource has to be used when creating a file in a particular iRODS path.
   For example:
   
   $ cat irodsResourceMap.conf 
   /CINECA01/home/cin_staff/rmucci00;resc-repl
   /CINECA01/home/cin_staff/mrossi;resc-repl

   If none of the listed paths is matched, the iRODS default resource is used. 

5) Modify the globus-gridftp-server script in /etc/init.d in order to create log
   and pid files where the user who is running the server has the required 
   ownership and run the server with:

   $ /etc/init.d/globus-gridftp-server start
   
   in alternative you can run the server with: 
   
   $ /usr/sbin/globus-gridftp-server -S -d ALL -c $WRKDIR/gridftp.conf


Additional configuration
--------------------------------
1) If desired, change the default home directory by setting the homeDirPattern
   environment variable in ````/etc/gridftp.conf````.  The pattern can reference up to
   two strings with ````%s````, first gets substituted with the zone name, second with
   the user name.  The default value is ````"/%s/home/%s"````, making the default
   directory ````/<zone>/home/<username>````.

Default configuration:

       $homeDirPattern "/%s/home/%s"

Example alternative configuration (defaulting to ````/<zone>/home````):

       $homeDirPattern "/%s/home"

2) Optionally, turn on the feature that would make the DSI module authenticate
   as the rods admin user - but operate under the privileges of the target user.

* Grant the GridFTP server access to the rods account:

        $ iadmin aua rods /C=TW/O=AP/OU=GRID/CN=irodsdev.canterbury.ac.nz
        $ iadmin lua rods
        rods /C=TW/O=AP/OU=GRID/CN=irodsdev.canterbury.ac.nz

* Make sure 'irodsUserName' is included in the '/path/to/.irodsEnv' file created above:

        irodsUserName rods

* Set the ````$irodsConnectAsAdmin````  environment variable in ````/etc/gridftp.conf```` to a non-empty value:

        $irodsConnectAsAdmin "rods"

  With all of this in place, it is no longer necessary to associate the DN of the server with each individual user - the server can now access user accounts through the rods account.

  NOTE: this feature requires iRODS server at least 3.3 - GSI authentication with a proxy user breaks on iRODS 3.2 and earlier.

3) Optionally, use a Globus gridmap callout module to map subject DNs to iRODS
   user names based on the existing mappings in iRODS (in r_user_auth table).
   Configuring this feature eliminates the need for a local grid map file - all
   user mappings can be done through the callout function.

   The gridmap callout configuration file gets already created as '$DEST_ETC_DIR/gridmap_iRODS_callout.conf'.

* To activate the module, set the '$GSI_AUTH_CONF' environment variable in '/etc/gridftp.conf' to point to the configuration file - already created as '$DEST_ETC_DIR/gridmap_iRODS_callout.conf'.

        $GSI_AUTHZ_CONF /etc/grid-security/gridmap_iRODS_callout.conf

* Note that in order for this module to work, the server certificate DN must be authorized to connect as a rodsAdmin user (e.g., the 'rods' user).

* This module also supports invoking an iRODS server-side command with iexec in
  case the DN does not have a mapping yet.  The command would receive the DN
  being mapped as a single argument and may for example add a mapping to an
  existing account, or create a new account.

  To enable this feature, set the '$irodsDnCommand' environment variable in
  '/etc/gridftp.conf' to the name of the command to execute.  On the iRODS
  server, the command should be installed in '$IRODS_HOME/server/bin/cmd/'.
  For example, to invoke a script called 'createUser', add:

        $irodsDnCommand "createUser"

* There is also a command line utility to test the mapping lookups (and script
  execution) that would otherwise be done by the gridmap module.  This utility
  command gets installed into '$DEST_BIN_DIR/testirodsmap' and should be
  invoked with the DN as a single argument.  The command would need to see the
  same environment variables as the gridmap module loaded into the GridFTP
  server - specifically, '$irodsEnvFile' pointing to the iRODS  environment and
  '$irodsDnCommand' setting the command to invoke if no mapping is found.  The
  'testirodsmap' command also needs to have access to the server host
  certificate - and find it either through the default mechanisms used by
  Globus GSI or by explicitly setting the 'X509_USER_CERT' and 'X509_USER_KEY'
  environment variables.  (The easiest way is to run the command in the same
  environment as the Globus GridFTP server, i.e., under the root account).  For
  example, invoke the command with:

        export irodsDnCommand=createUser 
        export irodsEnvFile=/path/to/.irodsEnv
        $DEST_BIN_DIR/testirodsmap "/C=XX/O=YYY/CN=Example User"

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

