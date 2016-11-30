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

cmake_minimum_required (VERSION 2.6)
project (iRODS_DSI C CXX)
set(GENERIC_LIB_VERSION "1.8")

##### Check ENV variables ######
if (DEFINED ENV{GLOBUS_LOCATION})
    message("GLOBUS_LOCATION is defined as $ENV{GLOBUS_LOCATION}")
else()
    message( FATAL_ERROR "GLOBUS_LOCATION is NOT defined, CMake will exit." )
endif()

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
if (DEFINED ENV{IRODS_PATH})
    message("IRODS_PATH is defined as $ENV{IRODS_PATH}")
    if ($ENV{IRODS_PATH} STREQUAL "/usr")
        add_definitions(-DIRODS_HEADER_HPP)
        set(irods_include_path_list "$ENV{IRODS_PATH}/include/irods")
        if (DEFINED ENV{IRODS_42_COMPAT})
            set(CMAKE_INSTALL_RPATH $ENV{IRODS_EXTERNALS_PATH}/clang-runtime3.8-0/lib)
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
            set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -stdlib=libc++")
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
            add_compile_options(-nostdinc++ -O0)

            set(CMAKE_C_COMPILER $ENV{IRODS_EXTERNALS_PATH}/clang3.8-0/bin/clang)
            set(CMAKE_CXX_COMPILER $ENV{IRODS_EXTERNALS_PATH}/clang3.8-0/bin/clang++)
            link_libraries(c++abi)

            remove_definitions(-DIRODS_HEADER_HPP)
            add_definitions(-DIRODS_42)
            if (DEFINED ENV{IRODS_EXTERNALS_PATH})
                set(irods_include_path_list "$ENV{IRODS_PATH}/include/irods"
                                    "$ENV{IRODS_EXTERNALS_PATH}/clang3.8-0/include/c++/v1")

                set(irods_link_obj_path
                    PRIVATE
                    irods_client
                    irods_plugin_dependencies
                    irods_common
                    "$ENV{IRODS_PATH}/lib/irods/plugins/network/libtcp_client.so"
                    "$ENV{IRODS_PATH}/lib/irods/plugins/auth/libnative_client.so" 
                    #"$ENV{IRODS_PATH}/lib/libirods_client.so"
                    #"$ENV{IRODS_PATH}/lib/libirods_common.so"
                    "-lrt"
                    #"-lcurl"
                    "$ENV{IRODS_EXTERNALS_PATH}/avro1.7.7-0/lib/libavrocpp.so"
                    "$ENV{IRODS_EXTERNALS_PATH}/jansson2.7-0/lib/libjansson.so"
                    "$ENV{IRODS_EXTERNALS_PATH}/boost1.60.0-0/lib/libboost_filesystem.so"
                    "$ENV{IRODS_EXTERNALS_PATH}/boost1.60.0-0/lib/libboost_regex.so"
                    "$ENV{IRODS_EXTERNALS_PATH}/boost1.60.0-0/lib/libboost_system.so"
                    "$ENV{IRODS_EXTERNALS_PATH}/boost1.60.0-0/lib/libboost_thread.so")
            else()
                message( FATAL_ERROR "IRODS_EXTERNALS_PATH is NOT defined, CMake will exit." )
            endif()
        elseif (DEFINED ENV{IRODS_40_COMPAT})
            set(irods_link_obj_path 
                    "$ENV{IRODS_PATH}/lib/irods/libirods_client.so"
                    "$ENV{IRODS_PATH}/lib/irods/libirods_client_api.so"
                    "-lrt"
                    "-lcurl"
                    "$ENV{IRODS_PATH}/lib/irods/libjansson.a"
                    "$ENV{IRODS_PATH}/lib/irods/libboost_filesystem.a"
                    "$ENV{IRODS_PATH}/lib/irods/libboost_regex.a"
                    "$ENV{IRODS_PATH}/lib/irods/libboost_system.a"
                    "$ENV{IRODS_PATH}/lib/irods/libboost_thread.a")
        else()
            set(irods_link_obj_path 
                    "$ENV{IRODS_PATH}/lib/libirods_client.so"
                    "$ENV{IRODS_PATH}/lib/libirods_client_api.so"
                    "-lrt"
                    "-lpthread"
                    "-pthread"
                    "-lssl"
                    "-lcurl"
                    "$ENV{IRODS_PATH}/lib/irods/externals/libjansson.a"
                    "$ENV{IRODS_PATH}/lib/irods/externals/libboost_chrono.a"
                    "$ENV{IRODS_PATH}/lib/irods/externals/libboost_filesystem.a"
                    "$ENV{IRODS_PATH}/lib/irods/externals/libboost_program_options.a"
                    "$ENV{IRODS_PATH}/lib/irods/externals/libboost_regex.a"
                    "$ENV{IRODS_PATH}/lib/irods/externals/libboost_system.a"
                    "$ENV{IRODS_PATH}/lib/irods/externals/libboost_thread.a")
        endif()
    else()
        set(irods_include_path_list "$ENV{IRODS_PATH}/lib/api/include"
                "$ENV{IRODS_PATH}/lib/core/include"
                "$ENV{IRODS_PATH}/server/icat/include"
                "$ENV{IRODS_PATH}/lib/md5/include/"
                "$ENV{IRODS_PATH}/lib/sha1/include/"
                "$ENV{IRODS_PATH}/server/core/include/"
                "$ENV{IRODS_PATH}/server/drivers/include/"
                "$ENV{IRODS_PATH}/server/re/include/")
        set(irods_link_obj_path "$ENV{IRODS_PATH}/lib/core/obj/libRodsAPIs.a")
    endif()
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
    set(gridmap_callout_library_name gridmap_iRODS_callout_$ENV{FLAVOR})
    set(gridmap_callout_library_LINK_FLAGS "-lglobus_gridmap_callout_error_$ENV{FLAVOR}")
else()
    message( WARNING "FLAVOR is NOT defined!" )
    set(dsi_library_name globus_gridftp_server_iRODS)
    set(gridmap_callout_library_name gridmap_iRODS_callout)
    set(gridmap_callout_library_LINK_FLAGS "-lglobus_gridmap_callout_error")
endif()

if (NOT DEFINED ENV{DEST_LIB_DIR})
    set(ENV{DEST_LIB_DIR} "$ENV{GLOBUS_LOCATION}/lib")
    # Note: this may have to be lib64 on 64-bit platforms.
endif()
message("DEST_LIB_DIR is defined as $ENV{DEST_LIB_DIR}")

if (NOT DEFINED ENV{DEST_ETC_DIR})
    if ($ENV{GLOBUS_LOCATION} STREQUAL "/usr")
        set(ENV{DEST_ETC_DIR} "/etc/grid-security")
    else()
        set(ENV{DEST_ETC_DIR} "$ENV{GLOBUS_LOCATION}/etc")
    endif()
endif()
message("DEST_ETC_DIR is defined as $ENV{DEST_ETC_DIR}")

if (NOT DEFINED ENV{DEST_BIN_DIR})
    set(ENV{DEST_BIN_DIR} "$ENV{GLOBUS_LOCATION}/bin")
endif()
message("DEST_BIN_DIR is defined as $ENV{DEST_BIN_DIR}")
#################################

message("DSI library name will be: ${dsi_library_name}")
message("Gridmap callout library name will be: ${gridmap_callout_library_name}")

add_library(${dsi_library_name} SHARED DSI/globus_gridftp_server_iRODS.c DSI/pid_manager.c external/cJSON.c)
add_library(${gridmap_callout_library_name} SHARED DSI/gridmap_iRODS_callout.c DSI/libirodsmap.c)

set_target_properties(${dsi_library_name} PROPERTIES VERSION ${GENERIC_LIB_VERSION} )
set_target_properties(${dsi_library_name} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${dsi_library_name} ${irods_link_obj_path})

set_target_properties(${gridmap_callout_library_name} PROPERTIES LINK_FLAGS ${gridmap_callout_library_LINK_FLAGS} VERSION ${GENERIC_LIB_VERSION} )
set_target_properties(${gridmap_callout_library_name} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${gridmap_callout_library_name} ${irods_link_obj_path})

set(gridmap_callout_conf_name gridmap_iRODS_callout.conf)

set(testirodsmap_exe "testirodsmap")
set(testirodsmap_exe_LINK_FLAGS "-ldl -lglobus_gss_assist -lstdc++")
add_executable(${testirodsmap_exe} DSI/testirodsmap.c DSI/libirodsmap.c)
set_target_properties(${testirodsmap_exe} PROPERTIES LINKER_LANGUAGE CXX)
set_target_propertieS(${testirodsmap_exe} PROPERTIES LINK_FLAGS ${testirodsmap_exe_LINK_FLAGS})
target_link_libraries(${testirodsmap_exe} ${irods_link_obj_path})

set(testpidmanager_exe "testpidmanager")
add_executable(${testpidmanager_exe} DSI/pid_manager_test.c DSI/pid_manager.c external/cJSON.c)
target_link_libraries(${testpidmanager_exe} ${irods_link_obj_path} "-lm -lglobus_gridftp_server")

set(CMAKE_C_FLAGS "-g -O2 -Wall -lstdc++ -lcurl -O3 -DNDEBUG")

include_directories(${irods_include_path_list})
include_directories($ENV{GLOBUS_LOCATION}/include/globus
        $ENV{GLOBUS_LOCATION}/lib/globus/include
        $ENV{GLOBUS_LOCATION}/lib64/globus/include
        DSI
        external)
if (DEFINED ENV{FLAVOR})
    include_directories($ENV{GLOBUS_LOCATION}/include/ENV{FLAVOR})
endif()

install(TARGETS ${dsi_library_name} ${gridmap_callout_library_name} DESTINATION $ENV{DEST_LIB_DIR})
install(TARGETS ${testirodsmap_exe} DESTINATION $ENV{DEST_BIN_DIR})
install(TARGETS ${testpidmanager_exe} DESTINATION $ENV{DEST_BIN_DIR})
configure_file(DSI/${gridmap_callout_conf_name}.in ${gridmap_callout_conf_name})
install(FILES ${gridmap_callout_conf_name} DESTINATION $ENV{DEST_ETC_DIR})

