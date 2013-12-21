#include <stdio.h>
#include <dlfcn.h>
#include <sys/stat.h>

#include <vector>
#include <algorithm>
#include <functional>

#include "send-response.h"

using std::vector;
using std::sort;
using std::greater;

void useLoadableMods(int socket, string filePath, string request_type, string query_string);
module * getModule(string newModName);
void set_arguments(char **arg, string query_string);
bool nameGreater(Dir_Element a, Dir_Element b);
bool nameLess(Dir_Element a, Dir_Element b);
bool dateGreater(Dir_Element a, Dir_Element b);
bool dateLess(Dir_Element a, Dir_Element b);
module *moduleListHead = NULL;


bool nameGreater(Dir_Element a, Dir_Element b) {
  if(a.name == b.name)
    return a.name < b.name;
  return a.name > b.name;
}

bool nameLess(Dir_Element a, Dir_Element b) {
  if(a.name == b.name)
    return a.name > b.name;
  return a.name < b.name;
}

bool dateGreater(Dir_Element a, Dir_Element b) {
  if(a.date == b.date)
    return a.date > b.date;
  return a.date < b.date;
}

bool dateLess(Dir_Element a, Dir_Element b) {
  if(a.date == b.date)
    return a.date < b.date;
  return a.date > b.date;
}

void writeRequest(int socket, int file, string contentType) {
  string header = "HTTP/1.1 200 Document follows\r\n"
                  "Server: CS 252 lab5\r\n" 
                  "Content-type: " + contentType + "\r\n"
                  "\r\n";

  write(socket, header.c_str(), header.length());
  char newChar;
  while (read(file, &newChar, 1) > 0) {
    write(socket, &newChar, 1); 
  }
}

void write404(int socket, string errorMessage) {
  if (errorMessage.empty()) {
    errorMessage = "File not Found";
  }

  string header = "HTTP/1.1 404 File Not Found\r\n"
                  "Server: CS 252 lab5\r\n"
                  "Content-type: text/plain\r\n"
                  "\r\n" +
                  errorMessage;

  write(socket, header.c_str(), header.length());
}

/* 
 * Begin browseDirectory
 */

void browseDirectory(int socket, DIR *dir, string docPath, string filePath, string query_string) {
  // Image tag declarations
  string folderImg = "<img src=\"/icons/menu.gif\" alt=\"[DIR]\">";
  string unknownImg = "<img src=\"/icons/unknown.gif\" alt=\"[DIR]\">";

  string header = "HTTP/1.0 200 Document Follows\r\n"
                "Server: CS 252 lab5\r\n"
                "Content-type: text/html\r\n"
                "\r\n";

  // Write the header to the socket
  write(socket, header.c_str(), header.length());

  // Swap Asc and Dsc every time same page is called. 
  const char *sortByName;
  const char *sortByTime;
  if (query_string == "C=N;O=D")
    sortByName = "C=N;O=A";
  else
    sortByName = "C=N;O=D";

  if (query_string == "C=M;O=D") 
    sortByTime = "C=M;O=A";
  else 
    sortByTime = "C=M;O=D";

  // Handles case where Directory called doesn't end in a / character.
  if (docPath[docPath.length()-1] != '/') docPath += "/";

  string htmlScript = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">\r\n"
                      "<html>\r\n"
                      "<head>\r\n"
                      "<body>\r\n"
                      "<h1>Index of " + filePath + 
                      "</h1>\r\n"
                      "<table><tr>"
                      "<th><a href=\"?" + sortByName + "\">Name</a></th>"
                      "<th><a href=\"?" + sortByTime + "\">Last modified</a></th>"
                      "</tr>"
                      "<tr><th colspan=\"5\"><hr></th></tr>"
                      "<tr><td valign=\"top\">" +
                      folderImg + 
                      "</td>"
                      "<td><a href=\"" +
                      docPath + ".."
                      "\">Parent Directory</a></td>"
                      "<td>&nbsp;</td>"
                      "<td align=\"right\">  - </td></tr>";

  // Extract the content from the directory.
  vector<Dir_Element> dirContent;
  struct dirent *ent;
  struct stat st;
  while ((ent = readdir(dir)) != NULL) {
    if (ent->d_name[0] != '.') {
      stat(filePath.c_str(), &st);
      char date[10];
      strftime(date, 10, "%d-%m-%y", gmtime(&(st.st_ctime)));
      struct Dir_Element de;
      de.name = (string)ent->d_name;
      de.date = (string)date;
      dirContent.push_back(de);
    }
  }

  // Determine what to sort the content by.
  int sortByIndex = query_string.find("=",0);
  char sortBy = query_string[sortByIndex+1];
  int orderIndex = query_string.find("=",sortByIndex+1);
  char order = query_string[orderIndex+1];

  if (sortBy == 'N') {
    if (order == 'D')
      sort(dirContent.begin(), dirContent.end(), nameGreater);
    else
      sort(dirContent.begin(), dirContent.end(), nameLess);
  }
  else {
    if (order == 'D')
      sort(dirContent.begin(), dirContent.end(), dateGreater);
    else
      sort(dirContent.begin(), dirContent.end(), dateLess);
  }

  // Make links to the sorted content and add it to the script.
  for (int i = 0; i < dirContent.size(); i++) {
    if (dirContent[i].name[0] == '.')
      continue;
    htmlScript += "<tr><td valign=\"top\">" +
                  unknownImg + 
                  "</td><td><a href=\"" + 
                  docPath + dirContent[i].name + "\">" +
                  dirContent[i].name + 
                  "</a></td>"
                  "<td>" + dirContent[i].date + "</td>"
                  "<td>&nbsp;</td>"
                  "<td align=\"right\">  - </td>"
                  "</tr>";
  }
  // Close all the tags.
  htmlScript += "<tr><th colspan=\"5\"><hr></th></tr>"
               "</table>"
               "</body>\r\n"
               "</html>\r\n";

  // Write the html to the specified socket. 
  write(socket,htmlScript.c_str(),htmlScript.length());
}

/* 
 * Begin writeCGI and related functions.
 */

void writeCGI(int socket, string filePath, string request_type, string query_string) {
  string response_header = "HTTP/1.1 200 Document follows\r\n"
                           "Server: Purdue CS Project\r\n";
                           
  write(socket, response_header.c_str(), response_header.length());

  // Get command and arguments
  char **arg = (char **)malloc(sizeof(char *)*20);
  arg[0] = (char *)filePath.c_str();

  set_arguments(arg, query_string);

  // Execute cgi script
  int ret = fork();
  if (ret == 0) {
    setenv("REQUEST_METHOD",request_type.c_str(),1);
    dup2(socket,1);
    close(socket);
    if (filePath.find(".so") != -1){
      useLoadableMods(socket, filePath, request_type, query_string);
    }
    else {
      int arglen = 5;
      execv(arg[0], arg);
      exit(1);
    }
  }
}

void set_arguments(char **arg, string query_string) {
  if (query_string.find("=") != -1) {
    setenv("QUERY_STRING", query_string.c_str(),1);
    arg[1] = NULL;
    arg[2] = 0;
  }
  else if (query_string.empty()) {
    arg[1] = NULL;
    arg[2] = 0;
  }
  else if (query_string.find("+") == -1) {
    arg[1] = (char *)query_string.c_str();
    arg[2] = 0;
  }
  else {
    int i = 0;
    int pos;
    int argCount = 1;
    while ((pos = query_string.find("+",i)) != -1) {
      arg[argCount++] = (char *)query_string.substr(i, pos-i-1).c_str();
      i = pos + 1;
    }
    arg[argCount] = 0;
  }
}
// Use loadable modules instead of execv()
void useLoadableMods(int socket, string filePath, string request_type, string query_string) {
  void * lib = dlopen((char *)filePath.c_str(), RTLD_LAZY);
  if (lib == NULL) {
    perror("dlopen");
  }
  httprun httprunmod;
  httprunmod = (httprun) dlsym(lib, "httprun");
  if (httprunmod == NULL) {
    perror( "dlsym: httprun not found:");
    exit(1);
  }
  httprunmod(socket, (char *)query_string.c_str());
  // unsetenv("REQUEST_METHOD");
}

module * getModule(string newModName) {
  module *newModule;
  module *temp = moduleListHead;
  if (moduleListHead == NULL) {
    void *lib = dlopen((char *)newModName.c_str(), RTLD_LAZY);
    newModule->name = newModName;
    newModule->lib = lib;
    newModule->next = NULL;
    moduleListHead = newModule;
    return newModule;
  }
  // Check if the module has already been opened.
  while (temp != NULL) {
    if (newModName == temp->name)
      return temp;
    temp = temp->next;
  }
  void *lib = dlopen((char *)newModName.c_str(), RTLD_LAZY);
  if (lib == NULL) {
    perror( "dlopen");
    exit(1);
  }
  else {
    newModule->name = newModName;
    newModule->lib = lib;
    temp->next = newModule;
  }
  return newModule;
}

/* 
 * End writeCGI and related functions.
 */



