#include <dirent.h>
#include <string>

using std::string;

struct Dir_Element {
  string name;
  string date;
};

struct module {
  string name;
  void *lib;
  module *next;
};

typedef void (*httprun)(int ssock, char* querystring);

void browseDirectory(int socket, DIR *d, string docPath, string filePath, string query_string);
void write404(int socket, string errorMessage);
void writeRequest(int socket, int file, string contentType);
void writeCGI(int socket, string filePath, string request_type, string query_string);