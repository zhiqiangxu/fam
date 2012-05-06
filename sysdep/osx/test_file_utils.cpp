#include "file_utils.h"
#include <iostream>

void print_file(const char *filename)
{
    std::cout << filename << std::endl;
}

int main(int argc, char *argv[])
{
    vfs::ForEachFile("/home/shore/lab/github/fam", &print_file);
}
