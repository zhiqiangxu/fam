#include "file_utils.h"
#include <iostream>

void print_file(const char *filename)
{
    std::cout << filename << std::endl;
}

int main(int argc, char *argv[])
{
    vfs::ForEachFile("/Users/xuzhiqiang/Develop/c/fam/", (void *)print_file);
    return 0;
}
