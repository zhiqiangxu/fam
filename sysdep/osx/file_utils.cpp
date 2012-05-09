#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <iostream>
#include "file_utils.h"

namespace vfs {


void ForEachFile(const char *dirname, void* cb, bool recursive, void* data)
{
    struct stat    st;
    struct dirent *dirent;
    DIR* dir = opendir(dirname);
    if (dir == NULL) 
        return;
    char fullpath[PATH_MAX];
    while ((dirent = readdir(dir)) != NULL)
    {
        if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
            continue;
        snprintf(fullpath, PATH_MAX, dirname[strlen(dirname) - 1] == '/' ? "%s%s" : "%s/%s", dirname, dirent->d_name);
        if (lstat(fullpath, &st) != 0) 
        {
            std::cout << "lstat failed upon " << fullpath << std::endl;
            continue;
        }
        if (data == NULL)
        {
            ((void (*)(const char *))cb)(fullpath);
        }
        else
        {
            ((void (*)(void*, const char *))cb)(data, fullpath);
        }
        if(recursive && S_ISDIR(st.st_mode)){
            ForEachFile(fullpath, cb, recursive, data);
        }
    }
    closedir(dir);
}

}
