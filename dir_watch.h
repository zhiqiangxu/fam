#include <vector>
#include <iostream>
#include <tr1/memory>

struct DirWatch;

typedef std::tr1::shared_ptr<DirWatch> PDirWatch;

class DirWatchNotification
{
public:
	enum EType
	{
		Created,
		Deleted,
		Changed
	};

	DirWatchNotification(std::string pathname, EType type)
		: pathname(pathname), type(type)
	{
	}

    std::string Pathname() 
	{
		return pathname;
	}

	EType Type() 
	{
		return type;
	}

private:
    std::string pathname;
	EType type;
};
typedef std::vector<DirWatchNotification> DirWatchNotifications;


int dir_watch_Add(const char* path, PDirWatch& dirWatch);

int dir_watch_Poll(DirWatchNotifications& notifications);
