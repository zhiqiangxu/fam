#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include "file_utils.h"

namespace vfs {

void ForEachFile(const char *dirname, void cb(const char *))
{
	char fullpath[PATH_MAX];
    struct stat    st;
    struct dirent *dirent;
    DIR* dir = opendir(dirname);
    if (dir == NULL) 
        return;
    while ((dirent = readdir(dir)) != NULL) 
    {
        if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
            continue;
	    snprintf(fullpath, PATH_MAX, "%s/%s", dirname, dirent->d_name);
	    if (lstat(fullpath, &st) != 0) 
        {
	        continue;
        }
	    if (S_ISDIR(st.st_mode)) 
        {
            cb(fullpath);
            ForEachFile(fullpath, cb);
        }
        else
        {
            cb(fullpath);
        }
    }
    closedir(dir);
}

}
