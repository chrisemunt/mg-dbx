# mg-dbx

High speed Synchronous and Asynchronous access to M-like databases from Node.js.

Chris Munt <cmunt@mgateway.com>  
10 October 2019, M/Gateway Developments Ltd [http://www.mgateway.com](http://www.mgateway.com)

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

       var dbx = require('mg-dbx').dbx;

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

#### Additional (optional) properties for the open() method

* **multithreaded**: A boolean value to be set to 'true' or 'false' (default **multithreaded: false**).  Set this property to 'true' if the application uses multithreaded techniques in JavaScript (e.g. V8 worker threads).


#### Returning (and optionally changing) the current directory (or Namespace)

       current_namespace = db.namespace([<new_namespace>]);

Example 1 (Get the current Namespace): 

       var nspace = db.namespace();

* Note this will return the current Namespace for InterSystems databases and the value of the current global directory for YottaDB (i.e. $ZG).

Example 2 (Change the current Namespace): 

       var new_nspace = db.namespace("SAMPLES");

* If the operation is successful this method will echo back the new Namespace name.  If not successful, the method will return the name of the current (unchanged) Namespace.

 
### Invocation of database commands

#### Register a global name (and fixed key)

       global = db.mglobal(<global_name>[, <fixed_key>]);

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


#### Increment the value of a global node

Synchronous:

       var result = <global>.increment(<key>, <increment_value>);

Asynchronous:

       <global>.increment(<key>, <increment_value>, callback(<error>, <result>));
      
Example (increment the value of the "counter" node by 1.5 and return the new value):

       var result = person.increment("counter", 1.5);


#### Lock a global node

Synchronous:

       var result = <global>.lock(<key>, <timeout>);

Asynchronous:

       <global>.lock(<key>, <timeout>, callback(<error>, <result>));
      
Example (lock global node '1' with a timeout of 30 seconds):

       var result = person.lock(1, 30);

* Note: Specify the timeout value as '-1' for no timeout (i.e. wait until the global node becomes available to lock).


#### Unlock a (previously locked) global node

Synchronous:

       var result = <global>.unlock(<key>);

Asynchronous:

       <global>.unlock(<key>, callback(<error>, <result>));
      
Example (unlock global node '1'):

       var result = person.unlock(1);


#### Reset a global name (and fixed key)

       <global>.reset(<global_name>[, <fixed_key>]);

Example:

       // Process orders for customer #1
       customer_orders = db.mglobal("Customer", 1, "orders")
       do_work ...

       // Process orders for customer #2
       customer_orders.reset("Customer", 2, "orders");
       do_work ...

 
### Cursor based data retrieval

This facility provides high-performance techniques for traversing records held in database globals. 

#### Specifying the query

The first task is to specify the 'query' for the global traverse.

       query = db.mglobalquery({global: <global_name>, key: [<seed_key>]}[, <options>]);

The 'options' object can contain the following properties:

* **multilevel**: A boolean value (default: **multilevel: false**). Set to 'true' to return all descendant nodes from the specified 'seed_key'.

* **getdata**: A boolean value (default: **getdata: false**). Set to 'true' to return any data values associated with each global node returned.

* **format**: Format for output (default: not specified). If the output consists of multiple data elements, the return value (by default) is a JavaScript object made up of a 'key' array and an associated 'data' value.  Set to "url" to return such data as a single URL escaped string including all key values ('key[1->n]') and any associated 'data' value.

Example (return all keys and names from the 'Person' global):

       query = db.mglobalquery({global: "Person", key: [""]}, {multilevel: false, getdata: true});

#### Traversing the dataset

In Key Order:

       result = query.next();

In Reverse Key Order:

       result = query.previous();

In all cases these methods will return 'null' when the end of the dataset is reached.

Example 1 (return all key values from the 'Person' global - returns a simple variable):

       query = db.mglobalquery({global: "Person", key: [""]});
       while ((result = query.next()) !== null) {
          console.log("result: " + result);
       }

Example 2 (return all key values and names from the 'Person' global - returns an object):

       query = db.mglobalquery({global: "Person", key: [""]}, multilevel: false, getdata: true);
       while ((result = query.next()) !== null) {
          console.log("result: " + JSON.stringify(result, null, '\t'));
       }


Example 3 (return all key values and names from the 'Person' global - returns a string):

       query = db.mglobalquery({global: "Person", key: [""]}, multilevel: false, getdata: true, format: "url"});
       while ((result = query.next()) !== null) {
          console.log("result: " + result);
       }

Example 4 (return all key values and names from the 'Person' global, including any descendant nodes):

       query = db.mglobalquery({global: "Person", key: [""]}, {{multilevel: true, getdata: true});
       while ((result = query.next()) !== null) {
          console.log("result: " + JSON.stringify(result, null, '\t'));
       }

* M programmers will recognise this last example as the M **$Query()** command.
 

#### Traversing the global directory (return a list of global names)

       query = db.mglobalquery({global: <seed_global_name>}, {globaldirectory: true});

Example (return all global names held in the current directory)

       query = db.mglobalquery({global: ""}, {globaldirectory: true});
       while ((result = query.next()) !== null) {
          console.log("result: " + result);
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


### Direct access to InterSystems classes (IRIS and Cache)

#### Invocation of a ClassMethod

Synchronous:

       result = db.classmethod(<class_name>, <classmethod_name>, <parameters>);

Asynchronous:

       db.function(<class_name>, <classmethod_name>, <parameters>, callback(<error>, <result>));
      
Example (Encode a date to internal storage format):

       result = db.classmethod("%Library.Date", "DisplayToLogical", "10/10/2019");

#### Creating and manipulating instances of objects

The following simple class will be used to illustrate this facility.

       Class User.Person Extends %Persistent
       {
          Property Number As %Integer;
          Property Name As %String;
          Property DateOfBirth As %Date;
          Method Age(AtDate As %Integer) As %Integer
          {
             Quit (AtDate - ..DateOfBirth) \ 365.25
          }
       }

#### Create an entry for a new Person

       person = db.classmethod("User.Person", "%New");

Add Data:

       result = person.setproperty("Number", 1);
       result = person.setproperty("Name", "John Smith");
       result = person.setproperty("DateOfBirth", "12/8/1995");

Save the object record:

       result = person.method("%Save");

#### Retrieve an entry for an existing Person

Retrieve data for object %Id of 1.
 
       person = db.classmethod("User.Person", "%OpenId", 1);

Return properties:

       var number = person.getproperty("Number");
       var name = person.getproperty("Name");
       var dob = person.getproperty("DateOfBirth");

Calculate person's age at a particular date:

       today = db.classmethod("%Library.Date", "DisplayToLogical", "10/10/2019");
       var age = person.method("Age", today);

#### Reusing an object container

Once created, it is possible to reuse containers holding previously instantiated objects using the **reset()** method.  Using this technique helps to reduce memory usage in the Node.js environment.

Example 1 Reset a container to hold a new instance:

       person.reset("User.Person", "%New");

Example 2 Reset a container to hold an existing instance (object %Id of 2):

       person.reset("User.Person", "%OpenId", 2);


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

### v1.1.5 (4 October 2019)

* Introduce global 'increment()' and 'lock(); methods.
* Introduce cursor based data retrieval.
* Introduce outline support for multithreading in JavaScript - **currently not stable!**.

### v1.2.6 (10 October 2019)

* Introduce support for direct access to InterSystems IRIS/Cache classes.
* Extend cursor based data retrieval to include an option for generating a global directory listing.
* Introduce a method to report and (optionally change) the current working global directory (or Namespace).
* Correct a fault that led to the timeout occasionally not being honoured in the **lock()** method.
* Correct a fault that led to Node.js exceptions not being processed correctly.

