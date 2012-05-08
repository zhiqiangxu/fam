#include <cstddef>

namespace vfs {

void ForEachFile(const char* dirname, void *, bool recursive = true, void* data = NULL);

}
