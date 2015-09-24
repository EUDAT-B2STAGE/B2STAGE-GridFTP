#include "pid_manager.h"

struct string {
  char *ptr;
  size_t len;
};

void init_string(struct string *s) {
    s->len = 0;
    s->ptr = malloc(s->len+1);
    if (s->ptr == NULL) {
        globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "iRODS DSI: manage_pid malloc() failed\n");
    }
    s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s)
{
    size_t new_len = s->len + size*nmemb;
    s->ptr = realloc(s->ptr, new_len+1);
    if (s->ptr == NULL) {
        globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "iRODS DSI: manage_pid realloc() failed\n");
    }
    memcpy(s->ptr+s->len, ptr, size*nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;

    return size*nmemb;
}


int manage_pid(char *pid_handle_URL, char *PID,  char **URL) {
    globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "iRODS DSI: manage_pid invoked..\n");
    CURL *curl;
    CURLcode res;
    struct string s;
 
    curl = curl_easy_init();


    if(curl) {
        init_string(&s);
        char* completeURL;
        
        unsigned int len = strlen(pid_handle_URL);
        // Remove last "/" from handle URL
        if (pid_handle_URL && pid_handle_URL[len - 1] == '/') 
        {
            pid_handle_URL[len - 1] = 0;
        }

        completeURL = malloc(strlen(pid_handle_URL)+strlen(PID)+1);
        strcpy(completeURL, pid_handle_URL);
        strcat(completeURL, PID);

        globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "iRODS DSI: complete handle URL: %s\n", completeURL);
        curl_easy_setopt(curl, CURLOPT_URL, completeURL);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
 
        // Perform the request, res will get the return code
        res = curl_easy_perform(curl);

        // Check for errors
        if(res != CURLE_OK)
        {
            globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "iRODS DSI: curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

            // always cleanup
            free(s.ptr);
            curl_easy_cleanup(curl);
            return res;
        }
        curl_easy_cleanup(curl);
        globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "iRODS DSI: JSON output from the Handle Server: %s\n", s.ptr);

        cJSON *root = cJSON_Parse(s.ptr);
        printf("JSON output: %s..\n", s.ptr);        
        int responseCode  = cJSON_GetObjectItem(root,"responseCode")->valueint;
        if (responseCode != 1)
        {
            globus_gfs_log_message(GLOBUS_GFS_LOG_INFO, "iRODS DSI: JSON responseCode =  %i\n", responseCode);
            free(s.ptr);
            return responseCode;
        }
        cJSON *values = cJSON_GetObjectItem(root,"values");
        int numberOfKeys = cJSON_GetArraySize(values);

        int i = 0;
        for (i = 0; i < numberOfKeys; i++)
        {
            cJSON *values_i = cJSON_GetArrayItem(values, i);
            char * mytype  = cJSON_GetObjectItem(values_i, "type")->valuestring;
            if ( strcmp("URL", mytype) == 0)
            {
	        cJSON *data = cJSON_GetObjectItem(values_i, "data");
	        char *myURL  = cJSON_GetObjectItem(data,"value")->valuestring;
	        *URL = malloc(strlen(myURL)+1);
	        strcpy(*URL, myURL);
	        free(s.ptr);
	        return 0;
	        break;
	    }
        }
        free(s.ptr);
        return 1;
    }
    return 1;
}


