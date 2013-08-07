include makefile_header
# include additional makefile headers here

# add needed cflags here
DSI_CFLAGS=$(GLOBUS_CFLAGS)

# add needed includes here
DSI_INCLUDES=$(GLOBUS_INCLUDES) -I /devel/products/irods/iRODS-3.0/lib/api/include -I /devel/products/irods/iRODS-3.0/lib/lib/core/include -I /devel/products/irods/iRODS-3.0/server/icat/include -I /devel/products/irods/iRODS-3.0/lib/core/include -I /devel/products/irods/iRODS-3.0/lib/md5/include/ -I /devel/products/irods/iRODS-3.0/server/core/include/ -I /devel/products/irods/iRODS-3.0/server/drivers/include/ -I /devel/products/irods/iRODS-3.0/server/re/include/

# added needed ldflags here
DSI_LDFLAGS=$(GLOBUS_LDFLAGS)

# add needed libraries here
DSI_LIBS= -L/devel/products/irods/iRODS-3.0/lib/core/obj -lRodsAPIs

FLAVOR=gcc64pthr

globus_gridftp_server_iRODS.o:
	$(GLOBUS_CC) $(DSI_CFLAGS) $(DSI_INCLUDES) \
		-shared -o libglobus_gridftp_server_iRODS_$(FLAVOR).so \
		globus_gridftp_server_iRODS.c -lstdc++\
		$(DSI_LDFLAGS) $(DSI_LIBS) -lpthread

install:
	cp -f libglobus_gridftp_server_iRODS_$(FLAVOR).so $(GLOBUS_LOCATION)/lib

clean:
	rm -f *.so
