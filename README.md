Http-Server
===========

This is a simple HTTP Server written in C++ for a linux machine

To use it in one window type:
  - Open up a terminal window.
  - Go to the root directory (Http-Server) and type make.
  - Then move to the bin folder and type the following:
      - myhttpd [-f|-t|-p] [<port>]  
              -f to run with processes  
              -t to run with threads  
              -p to run with thread pools 
            
      - Where 1024 < port < 65536. 
  
To test in another window type: 
  - telnet <host> <port>
  - Then GET <document> <crlf><crlf> to have the doc returned 
  - You can also type <machine_name>:<port>/<document> in a browser
    to have the file show up in the browser
