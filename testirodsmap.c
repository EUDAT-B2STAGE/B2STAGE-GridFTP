#include <stdio.h>
#include <unistd.h>

#include "libirodsmap.h"

int main(int argc, char **argv)
{
    char * user = NULL;
    char * zone = NULL;
    int rc = 0;
    if (argc>=2) {
        if ( (rc = get_irods_mapping(argv[1],&user,&zone, NULL, NULL)) == 0)
            fprintf(stdout, "Mapping for %s is %s#%s\n", argv[1], user, zone);
        else
	        fprintf(stderr, "Mapping for %s failed (%d)\n", argv[1], rc);
        return rc;
    } else {
        fprintf(stderr,"Usage: %s <DN>\n", argv[0]);
        return -2;
    };
}

void libirodsmap_log(int level, const char * message, const char * param, int status) {
    fprintf(stderr,"Level(%d) ", level);
    fprintf(stderr, message, param, status);
}

