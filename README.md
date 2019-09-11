# mg-dbx

High speed Synchronous and Asynchronous access to M-like databases from Node.js.

Chris Munt <cmunt@mgateway.com>  
7 September 2019, M/Gateway Developments Ltd [http://www.mgateway.com](http://www.mgateway.com)

* Verified to work with Node.js v4 to v12.
* [Release Notes](#RelNotes) can be found at the end of this document.

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

#### Register a global name (and fixed key)

       global := db.mglobal(<global_name>[, <fixed_key>])

Example (using a global named "Person"):

       var person = db.mglobal("Person");

#### Set a record

Synchronous:

       var result = <global>.set(<key>, <data>);

Asynchronous:

       <global>.set(<key>, <data>, callback(<error>, <result>));
      
Example:

       person.set(1, "John Smith");

#### Get a record

Synchronous:

       var result = <global>.get(<key>);

Asynchronous:

       <global>.set(<key>, callback(<error>, <result>));
      
Example:

       var name = person.get(1);

#### Delete a record

Synchronous:

       var result = <global>.delete(<key>);

Asynchronous:

       <global>.delete(<key>, callback(<error>, <result>));
      
Example:

       var name = person.delete(1);


#### Check whether a record is defined

Synchronous:

       var result = <global>.defined(<key>);

Asynchronous:

       <global>.defined(<key>, callback(<error>, <result>));
      
Example:

       var name = person.defined(1);


#### Parse a set of records (in order)

Synchronous:

       var result = <global>.next(<key>);

Asynchronous:

       <global>.next(<key>, callback(<error>, <result>));
      
Example:

       var key = "";
       while ((key = person.next(key)) != "") {
          console.log("\nPerson: " + key + ' : ' + person.get(key));
       }


#### Parse a set of records (in reverse order)

Synchronous:

       var result = <global>.previous(<key>);

Asynchronous:

       <global>.previous(<key>, callback(<error>, <result>));
      
Example:

       var key = "";
       while ((key = person.previous(key)) != "") {
          console.log("\nPerson: " + key + ' : ' + person.get(key));
       }


#### Reset a global name (and fixed key)

       <global>.reset(<global_name>[, <fixed_key>]);

Example:

       // Process orders for customer #1
       customer_orders = db.mglobal("Customer", 1, "orders")
       do_work ...

       // Process orders for customer #2
       customer_orders.reset("Customer", 2, "orders");
       do_work ...


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

## <a name="RelNotes"></a>Release Notes

### v1.0.3 (28 June 2019)

* Initial Release

### v1.0.4 (7 September 2019)

* Allow a global to be registered with a fixed leading key (i.e. leading fixed subscripts).
* Introduce a method to reset a global name (and any associated fixed keys).

