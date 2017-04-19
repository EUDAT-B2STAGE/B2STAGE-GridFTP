B2STAGE-GridFTP (iRODS-DSI)
===============

B2STAGE service core code for EUDAT project: DSI interface


GridFTP is a high-performance, secure, reliable data transfer protocol which provides remote access to data stores. 
There are many different types of data storage systems from standard file systems to arrays of magnetic tape: to allow GridFTP to be used with as many data storage systems as possible, the GridFTP can be extended, implementing an interface called Data Storage Interface (DSI).

The GridFTP iRODS DSI consists of C functions which can interact with iRODS through the iRODS C API. The main supported operations are get, put, delete and list.

![Alt text](/images/iRODS-DSI.png?raw=true "iRODS-DSI")

Once installed and configured, users will be able to interact with iRODS through 
any GridFTP client passing to it a valid iRODS path; for instance:
 
B2STAGE-GridFTP (iRODS-DSI)
===============

B2STAGE service core code for EUDAT project: DSI interface

GridFTP is a high-performance, secure, reliable data transfer protocol which provides remote access to data stores. 
There are many different types of data storage systems from standard file systems to arrays of magnetic tape: to allow GridFTP to be used with as many data storage systems as possible, the GridFTP can be extended, implementing an interface called Data Storage Interface (DSI).

The GridFTP iRODS DSI consists of C functions which can interact with iRODS through the iRODS C API. The main supported operations are get, put, delete and list.

![Alt text](/images/iRODS-DSI.png?raw=true "iRODS-DSI")

Once installed and configured, users will be able to interact with iRODS through 
any GridFTP client passing to it a valid iRODS path; for instance:
```
$ globus-url-copy -list gsiftp://develvm.cluster.cineca.it:2811/tempZone/home/myuser/
```
 
will list the content of the */tempZone/home/myuser/* iRODS collection.

The module can be loaded by the GridFTP server at start-up time through a specific command line option. Therefore, no changes are required in the GridFTP server installation. The decoupling from the possible future changes to the server simplifies the maintenance of the software module.


Building iRODS DSI with CMake
--------------------------------

**Prerequisites:**

- CMake 2.7 or higher

- iRODS with the Development Tools and Runtime Libraries packages: follow the instructions at https://packages.irods.org/ to add the iRODS repository to your package manager. Installation instructions can be found at https://irods.org/download/

- For 4.2, the iRODS external packages need to be installed. These provide a consistent build environment (cmake, clang, etc.) to build the GridFTP plugin.

 Ubuntu:

 ```
 sudo apt-get install 'irods-externals*
 ```

 Centos:
 ```
 sudo yum install 'irods-externals*
 ```

- Globus and other packages:
 Ubuntu:
 ```
 sudo apt-get install -y globus-gridftp-server-progs globus-simple-ca globus-gass-copy-progs libglobus-gss-assist-dev
 sudo apt-get install -y libglobus-common-dev libglobus-gridftp-server-dev libglobus-gridmap-callout-error-dev
 sudo apt-get install -y libcurl4-openssl-dev
 sudo apt-get install -y git
 sudo apt-get install -y g++
 sudo apt-get install -y dpkg-dev
 sudo apt-get install -y cdbs
 sudo apt-get install -y globus-gsi-cert-utils-progs
 sudo apt-get install -y globus-proxy-utils
 ```
 Centos:
 ```
 sudo yum install -y globus-gridftp-server-progs globus-simple-ca globus-gass-copy-progs libglobus-gss-assist-dev
 sudo yum install -y globus-common-devel globus-gridftp-server-devel   globus-gridmap-callout-error-devel
 sudo yum install -y libcurl-devel
 sudo yum install -y git
 sudo yum install -y g++
 sudo yum install -y globus-gsi-cert-utils-progs
 sudo yum install -y globus-proxy-utils
 ```
 
----------

1. Clone this repository
 ```
 git clone https://github.com/EUDAT-B2STAGE/B2STAGE-GridFTP.git
 ```
    
2. Create a deployment folder:
 ```
 mkdir /<preferred_path>/iRODS_DSI
 ```
 
3. Set some environment variables that are used by the build process and the PATH so that the correct version of cmake will be found.
 ```
 export PATH=/opt/irods-externals/cmake3.5.2-0/bin:$PATH
 export GLOBUS_LOCATION="/usr"
 export IRODS_PATH="/usr"
 export DEST_LIB_DIR="/<preferred_path>/iRODS_DSI"
 export DEST_BIN_DIR="/<preferred_path>/iRODS_DSI"
 export DEST_ETC_DIR="/<preferred_path>/iRODS_DSI"
 export IRODS_EXTERNALS_PATH=/opt/irods-externals
 ```
 In Ubuntu you may need to set up C_INCLUDE_PATH so that the globus_config.h header can be found.
 ```
 export C_INCLUDE_PATH=/usr/include/x86_64-linux-gnu/globus
 ```

 If you are building for iRODS 4.2, set the IRODS_42_COMPAT environment variable to true.
 ```
 export IRODS_42_COMPAT=true
 ```

4. Build and install the iRODS-DSI:
 ```
 cd B2STAGE-GridFTP
 cmake .
 make install
 ```


Configuring the GridFTP server and run
--------------------------------

1. As the user who runs the GridFTP server, create the file *~/.irods/irods_environment.json* (or *~/.irods/.irodsEnv* for iRODS < 4.1.x) and populate it with the information related to a rodsadmin" user; for instance:
 ```
 {
       "irods_host" : "irods4", 
       "irods_port" : 1247,
	   "irods_user_name" : "rods",
	   "irods_zone_name" : "tempZone",
	   "irods_default_resource" : "demoResc"
 }
 ```
	Note that the *"irods_host"* and *"irods_port"* identify the iRODS server that the DSI will contact during each request. Be sure to set the *irods_default_resource*, this variable is not set when you create the file with *iinit* or when you copy it over from another user.

2. As the user who runs the GridFTP server, try an `ils` icommand to verify that the information set in the *irods_environment.json* are fine. If needed, perform an `iinit` to authenticate the iRODS user. 

3. Add the following lines to the GridFTP configuration file (typically $GLOBUS_LOCATION/etc/gridftp.conf*):


 ```
 $LD_LIBRARY_PATH "$LD_LIBRARY_PATH:/<preferred_path>/iRODS_DSI"
 $irodsConnectAsAdmin "rods"
 load_dsi_module iRODS 
 auth_level 4
 ```
 In case the GridFTP is run as a system service, also set the $HOME env variable pointing to the home folder of the user who runs the gridftp server:
 ```
 $HOME /path/to/user/home
 ```

4. Add the following line at the beginning of the *globus-gridftp-server* (usually */etc/init.d/globus-gridftp-server*) file:
 ```
 export $LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/<preferred_path>/iRODS_DSI/"
 ```

5. When deploying the DSI with iRODS 4.1, it is necessary to load the GridFTP server library alongside the DSI library by adding the following lines at the beginning of the *globus-gridftp-server*  (usually */etc/init.d/globus-gridftp-server*) file:

 Ubuntu:
 ```
 export LD_PRELOAD="$LD_PRELOAD:/usr/lib/x86_64-linux-gnu/libglobus_gridftp_server.so:/iRODS_DSI/libglobus_gridftp_server_iRODS.so"
 ```

 Centos:
 ```
 export LD_PRELOAD="$LD_PRELOAD:/usr/lib64/libglobus_gridftp_server.so:/iRODS_DSI/libglobus_gridftp_server_iRODS.so"
 ```
     
6. To enable the GridFTP CKSM (checksum) command to interoperate with Globus.org, it is necessary to configure iRODS to use MD5 checksums (iRODS 4 otherwise defaults to SHA-256). Edit */etc/irods/server_config.json* and set:
 ```
 "default_hash_scheme": "MD5",
 ```

7. Run the server with:
 ```
 /etc/init.d/globus-gridftp-server restart
 ```


Additional configuration
--------------------------------

1. Optionally, it is possible to enable the DSI module to manage the input path as a PID: the DSI will try to resolve the PID and perform the requested operation using the URI returned by the Handle server (currently only listing and downloading is supported).
 For instance: 
 ```
 globus-url-copy -list  gsiftp://develvm01.pico.cineca.it:2811/11100/da3dae0a-6371-11e5-ba64-a41f13eb32b2/
 ```
 will return the list of objects contained in the collection to which the PID "11100/da3dae0a-6371-11e5-ba64-a41f13eb32b2/" points, or:
 ```
 globus-url-copy gsiftp://develvm01.pico.cineca.it:2811/11100/xa3dae0a-6371-11e5-ba64-a41f13eb32b1 /test.txt
 ```
 will download the object pointed by the PID "11100/xa3dae0a-6371-11e5-ba64-a41f13eb32b1".
    
 If the PID resolution fails (either because the Handle server cannot resolve the PID or because the path passed as input is not a PID) the DSI will try to perform the requested operation anyway, using the original input path. This guarantees that the DSI can accept both PIDs and standard iRODS paths.
    
 To enable the PID resolution export the address of your handle-resolver to the GridFTP configuration file (typically *$GLOBUS_LOCATION/etc/gridftp.conf*):
 ```
 $pidHandleServer "http://hdl.handle.net/api/handles"
 ```
 If you are using a different resolver than the global handle resolver, replace *hdl.handle.net* with the correct address.

 Note: Once the PID is correctly resolved, the requested operation (listing or downloading) will be correctly performed only if the URI returned by the Handle server is a valid iRODS path pointing to the iRODS instance to which the DSI is connected to.

2. If desired, change the default home directory by setting the homeDirPattern environment variable in  
 ```
 $GLOBUS_LOCATION/etc/gridftp.conf
 ```
 The pattern can reference up to two strings with `%s`, first gets substituted with the zone name, second with the user name.  The default alue is `"/%s/home/%s"`, making the default directory `/<zone>/home/<username>`.

 Default configuration:
 ```
 $homeDirPattern "/%s/home/%s"
 ```

 Example alternative configuration (defaulting to `/<zone>/home`):
 ```
 $homeDirPattern "/%s/home"
 ```
        
3. It is possible to specify a policy to manage more than one iRODS resource setting the irodsResourceMap environment variable in `$GLOBUS_LOCATION/etc/gridftp.conf`.
 ```
 $irodsResourceMap "path/to/mapResourcefile"
 ```

 The irodsResourceMap variable must point to a file which specifies which iRODS resource has to be used when uploading or downloading a file in a particular iRODS path.
 For example:
 ```
 $cat path/to/mapResourcefile

 /CINECA01/home/cin_staff/rmucci00;resc-repl
 /CINECA01/home/cin_staff/mrossi;resc-repl
 ```
 If none of the listed paths is matched, the iRODS default resource is used. 

4. Optionally, use a Globus gridmap callout module to map subject DNs to iRODS user names based on the existing mappings in iRODS (in r_user_auth table). 
 Configuring this feature eliminates the need for a local grid map file - all user mappings can be done through the callout function.

 The gridmap callout configuration file gets already created as '/preferred_path/iRODS_DSI/gridmap_iRODS_callout.conf'.

 To activate the module, set the '$GSI_AUTHZ_CONF' environment variable in `$GLOBUS_LOCATION/etc/gridftp.conf` to point to the configuration file already created as '/preferred_path/iRODS_DSI/gridmap_iRODS_callout.conf'.
 ```
 $GSI_AUTHZ_CONF /preferred_path/iRODS_DSI/gridmap_iRODS_callout.conf
 ```
        
 Note that in order for this module to work, the server certificate DN must be authorized to connect as a rodsAdmin user (e.g., the 'rods' user).


Additional notes
---------------------------------

This module also supports invoking an iRODS server-side command with iexec in case the DN does not have a mapping yet. The command would receive the DN being mapped as a single argument and may for example add a mapping to an existing account, or create a new account.
To enable this feature, set the `$irodsDnCommand` environment variable in '/etc/gridftp.conf' to the name of the command to execute. On the iRODS server, the command should be installed in '$IRODS_HOME/server/bin/cmd/'. 
For example, to invoke a script called 'createUser', add:
```
$irodsDnCommand "createUser"
```

There is also a command line utility to test the mapping lookups (and script execution) that would otherwise be done by the gridmap module. This utility command gets installed into '$DEST_BIN_DIR/testirodsmap' and should be invoked with the DN as a single argument. 
The command would need to see the same environment variables as the gridmap module loaded into the GridFTP server - specifically, `$irodsEnvFile` pointing to the iRODS environment and '$irodsDnCommand' setting the command to invoke if no mapping is found. The 'testirodsmap' command also needs to have access to the server host certificate - and find it either through the default mechanisms used by Globus GSI or by explicitly setting the 
'X509_USER_CERT' and 'X509_USER_KEY' environment variables.
(The easiest way is to run the command in the same environment as the Globus GridFTP server, i.e., under the root account). For example, invoke the command with:
```
export irodsDnCommand=createUser 
export irodsEnvFile=/path/to/.irodsEnv
$DEST_BIN_DIR/testirodsmap "/C=XX/O=YYY/CN=Example User"
```


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
$ globus-url-copy -list gsiftp://develvm.cluster.cineca.it:2811/tempZone/home/myuser/`
