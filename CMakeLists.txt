##############################################################################
# Copyright (c) 2013 CINECA (www.hpc.cineca.it)
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
# http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#
# Globus DSI to manage data on iRODS.
#
# Author: Roberto Mucci - SCAI - CINECA
# Email:  hpc-service@cineca.it
#
###############################################################################

cmake_minimum_required (VERSION 2.8)
project (iRODS_DSI C)
set(GENERIC_LIB_VERSION "1.6.3")

##### Check ENV variables ######
if (DEFINED ENV{GLOBUS_LOCATION})
    message("GLOBUS_LOCATION is defined as $ENV{GLOBUS_LOCATION}")
else()
    message( FATAL_ERROR "GLOBUS_LOCATION is NOT defined, CMake will exit." )
endif()

if (DEFINED ENV{IRODS_PATH})
    message("IRODS_PATH is defined as $ENV{IRODS_PATH}")
else()
    message( FATAL_ERROR "IRODS_PATH is NOT defined, CMake will exit." )
endif()

if (DEFINED ENV{RESOURCE_MAP_PATH})
    message("RESOURCE_MAP_PATH is defined as $ENV{RESOURCE_MAP_PATH}")
else()
    message( WARNING "RESOURCE_MAP_PATH is NOT defined!" )
endif()

if (DEFINED ENV{FLAVOR})
    message("FLAVOR is defined as $ENV{FLAVOR}")
    set(dsi_library_name globus_gridftp_server_iRODS_$ENV{FLAVOR})
else()
    message( WARNING "FLAVOR is NOT defined!" )
    set(dsi_library_name globus_gridftp_server_iRODS)
endif()

if (NOT DEFINED ENV{DEST_LIB_DIR})
    set(ENV{DEST_LIB_DIR} "$ENV{GLOBUS_LOCATION}/lib")
endif()
message("DEST_LIB_DIR is defined as $ENV{DEST_LIB_DIR}")
#################################

message("DSI library name will be: ${dsi_library_name}")

add_library(${dsi_library_name} SHARED globus_gridftp_server_iRODS.c)

set_target_properties( ${dsi_library_name} PROPERTIES VERSION ${GENERIC_LIB_VERSION} )
target_link_libraries (${dsi_library_name} $ENV{IRODS_PATH}/lib/core/obj/libRodsAPIs.a)

set(CMAKE_C_FLAGS "-g -O2 -Wall -lstdc++")

include_directories($ENV{IRODS_PATH}/lib/api/include  
$ENV{IRODS_PATH}/lib/core/include
$ENV{IRODS_PATH}/server/icat/include 
$ENV{IRODS_PATH}/lib/md5/include/ 
$ENV{IRODS_PATH}/lib/sha1/include/
$ENV{IRODS_PATH}/server/core/include/ 
$ENV{IRODS_PATH}/server/drivers/include/ 
$ENV{IRODS_PATH}/server/re/include/ 
$ENV{GLOBUS_LOCATION}/include/globus 
$ENV{GLOBUS_LOCATION}/include/gcc64pthr
$ENV{GLOBUS_LOCATION}/lib/globus/include
$ENV{GLOBUS_LOCATION}/lib64/globus/include)

add_definitions(-DIRODS_MAPS_PATH=\"$ENV{RESOURCE_MAP_PATH}\")
install(TARGETS ${dsi_library_name} DESTINATION $ENV{DEST_LIB_DIR})

