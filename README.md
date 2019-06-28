# mg-dbx

High speed Synchronous and Asynchronous access to M-like databases from Node.js.

Chris Munt <cmunt@mgateway.com>  
28 June 2019, M/Gateway Developments Ltd [http://www.mgateway.com](http://www.mgateway.com)


## Pre-requisites 

**mg-dbx** is a Node.js addon written in C++.  It is distributed as C++ source code and the NPM installation procedure will expect a C++ compiler to be present on the target system.

Linux systems can use the freely available GNU C++ compiler (g++) which can be installed as follows.

Ubuntu:

       apt-get install g++

Red Hat and CentOS:

       yum install gcc-c++

Apple OS X can use the freely available **Xcode** development environment.

There are two options for Windows, both of which are free:

* Microsoft Visual Studio Community: [https://www.visualstudio.com/vs/community/](https://www.visualstudio.com/vs/community/)
* MinGW: [http://www.mingw.org/](http://www.mingw.org/)

If the Windows machine is not set up for systems development, building native Addon modules for this platform from C++ source can be quite arduous.  There is some helpful advice available at:

* [Compiling native Addon modules for Windows](https://github.com/Microsoft/nodejs-guidelines/blob/master/windows-environment.md#compiling-native-addon-modules)

Alternatively there are built Windows x64 binaries available from:

* [https://github.com/chrisemunt/mg-dbx/blob/master/bin/winx64](https://github.com/chrisemunt/mg-dbx/blob/master/bin/winx64)

## Installing mg-dbx

Assuming that Node.js is already installed and a C++ compiler is available to the installation process:

       npm install mg-dbx

This command will create the **mg-dbx** addon (*mg-dbx.node*).

## Documentation

Most **mg-dbx** methods are capable of operating either synchronously or asynchronously. For an operation to complete asynchronously, simply supply a suitable callback as the last argument in the call.

The first step is to add **mg-dbx** to your Node.js script

       var dbx = require('mg-dbx');

### Create a Server Object

       var db = new dbx();


### Open a connection to the database

In the following examples, modify all paths (and any user names and passwords) to match those of your own installation.

#### InterSystems Cache

Assuming Cache is installed under **/opt/cache20181/**

           var open = db.open({
               type: "Cache",
               path:"/opt/cache20181/mgr",
               username: "_SYSTEM",
               password: "SYS",
               namespace: "USER"
             });


#### InterSystems IRIS

Assuming IRIS is installed under **/opt/IRIS20181/**

           var open = db.open({
               type: "IRIS",
               path:"/opt/IRIS20181/mgr",
               username: "_SYSTEM",
               password: "SYS",
               namespace: "USER"
             });

#### YottaDB

Assuming an 'out of the box' YottaDB installation under **/usr/local/lib/yottadb/r122**.

           var envvars = "";
           envvars = envvars + "ydb_dir=/root/.yottadb\n"
           envvars = envvars + "ydb_rel=r1.22_x86_64\n"
           envvars = envvars + "ydb_gbldir=/root/.yottadb/r1.22_x86_64/g/yottadb.gld\n"
           envvars = envvars + "ydb_routines=/root/.yottadb/r1.22_x86_64/o*(/root/.yottadb/r1.22_x86_64/r /root/.yottadb/r) /usr/local/lib/yottadb/r122/libyottadbutil.so\n"
           envvars = envvars + "ydb_ci=/usr/local/lib/yottadb/r122/cm.ci\n"
           envvars = envvars + "\n"

           var open = db.open({
               type: "YottaDB",
               path: "/usr/local/lib/yottadb/r122",
               env_vars: envvars
             });


### Invocation of database commands

#### Register a global name

      var person = db.mglobal("Person");

#### Set a record

Synchronous:

       var result = person.set(<key>, <data>);

Asynchronous:

       person.set(<key>, <data>, callback(<error>, <result>));
      
Example:

       person.set(1, "John Smith");

#### Get a record

Synchronous:

       var result = person.get(<key>);

Asynchronous:

       person.set(<key>, callback(<error>, <result>));
      
Example:

       var name = person.get(1);

#### Delete a record

Synchronous:

       var result = person.delete(<key>);

Asynchronous:

       peeson.delete(<key>, callback(<error>, <result>));
      
Example:

       var name = person.delete(1);


#### Check whether a record is defined

Synchronous:

       var result = person.defined(<key>);

Asynchronous:

       person.defined(<key>, callback(<error>, <result>));
      
Example:

       var name = person.defined(1);


#### Parse a set of records (in order)

Synchronous:

       var result = person.next(<key>);

Asynchronous:

       person.next(<key>, callback(<error>, <result>));
      
Example:

       var key = "";
       while ((key = person.next(key)) != "") {
          console.log("\nPerson: " + key + ' : ' + person.get(key));
       }


#### Parse a set of records (in reverse order)

Synchronous:

       var result = person.previous(<key>);

Asynchronous:

       person.previous(<key>, callback(<error>, <result>));
      
Example:

       var key = "";
       while ((key = person.previous(key)) != "") {
          console.log("\nPerson: " + key + ' : ' + person.get(key));
       }


### Invocation of database functions

Synchronous:

       result = db.function(<function>, <parameters>);

Asynchronous:

       db.function(<function>, <parameters>, callback(<error>, <result>));
      
Example:

M routine called 'math':

       add(a, b) ; Add two numbers together
                 quit (a+b)

JavaScript invocation:

      result = db.function("add^math", 2, 3);


### Return the version of mg-dbx

       var result = db.version();

Example:

       console.log("\nmg-dbx Version: " + db.version());



### Close database connection

       db.close();

## License

Copyright (c) 2018-2019 M/Gateway Developments Ltd,
Surrey UK.                                                      
All rights reserved.
 
http://www.mgateway.com                                                  
Email: cmunt@mgateway.com
 
 
Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.      

