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


Building iRODS DSI with CMake
--------------------------------

Prerequisite: 

- CMake 2.8 or higher

- globus-gridftp-server-progs
- libglobus-common-dev (.deb) or globus-common-devel (.rpm)
- libglobus-gridftp-server-dev (.deb) or globus-gridftp-server-devel (.rpm)
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
   relocation R_X86_64_32 against `.bss' can not be used when making a
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
   iRODS again. The solution 'b' is the same as solution 'a', but using irodssetup 
   instead of make. The solution 'c' is faster to be implemented. 

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



Logrotate
--------------------------------
If you use -d ALL as in the example, please, be aware that the log files could 
grow quite a lot so the use of logrotate is suggested. 


