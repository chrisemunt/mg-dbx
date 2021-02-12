# mg-dbx

High speed Synchronous and Asynchronous access to InterSystems Cache/IRIS and YottaDB from Node.js.

Chris Munt <cmunt@mgateway.com>  
12 February 2021, M/Gateway Developments Ltd [http://www.mgateway.com](http://www.mgateway.com)

* Verified to work with Node.js v8 to v15.
* Two connectivity models to the InterSystems or YottaDB database are provided: High performance via the local database API or network based.
* [Release Notes](#RelNotes) can be found at the end of this document.

Contents

* [Pre-requisites](#PreReq") 
* [Installing mg-dbx](#Install)
* [Connecting to the database](#Connect)
* [Invocation of database commands](#DBCommands)
* [Invocation of database functions](#DBFunctions)
* [Transaction Processing](#TProcessing)
* [Direct access to InterSystems classes (IRIS and Cache)](#DBClasses)
* [Direct access to SQL: MGSQL and InterSystems SQL (IRIS and Cache)](#DBSQL)
* [Working with binary data](#Binary)
* [Using Node.js/V8 worker threads](#Threads)
* [The Event Log](#EventLog)
* [License](#License)

## <a name="PreReq"></a> Pre-requisites 

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

## <a name="Install"></a> Installing mg-dbx

Assuming that Node.js is already installed and a C++ compiler is available to the installation process:

       npm install mg-dbx

This command will create the **mg-dbx** addon (*mg-dbx.node*).


### Installing the M support routines (also known as the DB Superserver)

The M support routines are required for:

* Network based access to databases.
* Direct access to SQL (either via the API or via the network).
* The Merge command under YottaDB (either via the API or via the network).

Two M routines need to be installed (%zmgsi and %zmgsis).  These can be found in the *Service Integration Gateway* (**mgsi**) GitHub source code repository ([https://github.com/chrisemunt/mgsi](https://github.com/chrisemunt/mgsi)).  Note that it is not necessary to install the whole *Service Integration Gateway*, just the two M routines held in that repository.

#### Installation for InterSystems Cache/IRIS

Log in to the %SYS Namespace and install the **zmgsi** routines held in **/isc/zmgsi\_isc.ro**.

       do $system.OBJ.Load("/isc/zmgsi_isc.ro","ck")

Change to your development UCI and check the installation:

       do ^%zmgsi

       M/Gateway Developments Ltd - Service Integration Gateway
       Version: 4.0; Revision 16 (11 February 2021)


#### Installation for YottaDB

The instructions given here assume a standard 'out of the box' installation of **YottaDB** (version 1.30) deployed in the following location:

       /usr/local/lib/yottadb/r130

The primary default location for routines:

       /root/.yottadb/r1.30_x86_64/r

Copy all the routines (i.e. all files with an 'm' extension) held in the GitHub **/yottadb** directory to:

       /root/.yottadb/r1.30_x86_64/r

Change directory to the following location and start a **YottaDB** command shell:

       cd /usr/local/lib/yottadb/r130
       ./ydb

Link all the **zmgsi** routines and check the installation:

       do ylink^%zmgsi

       do ^%zmgsi

       M/Gateway Developments Ltd - Service Integration Gateway
       Version: 4.0; Revision 16 (11 February 2021)

Note that the version of **zmgsi** is successfully displayed.

Finally, add the following lines to the interface file (**zmgsi.ci** in the example used in the db.open() method).

       sqlemg: ydb_string_t * sqlemg^%zmgsis(I:ydb_string_t*, I:ydb_string_t *, I:ydb_string_t *)
       sqlrow: ydb_string_t * sqlrow^%zmgsis(I:ydb_string_t*, I:ydb_string_t *, I:ydb_string_t *)
       sqldel: ydb_string_t * sqldel^%zmgsis(I:ydb_string_t*, I:ydb_string_t *)
       ifc_zmgsis: ydb_string_t * ifc^%zmgsis(I:ydb_string_t*, I:ydb_string_t *, I:ydb_string_t*)

A copy of this file can be downloaded from the **/unix** directory of the  **mgsi** GitHub repository [here](https://github.com/chrisemunt/mgsi)

### Starting the DB Superserver (for network based connectivity only)

The default TCP server port for **zmgsi** is **7041**.  If you wish to use an alternative port then modify the following instructions accordingly.

* For InterSystems DB servers the concurrent TCP service should be started in the **%SYS** Namespace.

Start the DB Superserver using the following command:

       do start^%zmgsi(0) 

To use a server TCP port other than 7041, specify it in the start-up command (as opposed to using zero to indicate the default port of 7041).

* For YottaDB, as an alternative to starting the DB Superserver from the command prompt, Superserver processes can be started via the **xinetd** daemon.  Instructions for configuring this option can be found in the **mgsi** repository [here](https://github.com/chrisemunt/mgsi)


## <a name="Connect"></a> Connecting to the database

Most **mg-dbx** methods are capable of operating either synchronously or asynchronously. For an operation to complete asynchronously, simply supply a suitable callback as the last argument in the call.

The first step is to add **mg-dbx** to your Node.js script

       var dbx = require('mg-dbx').dbx;

And optionally (as required):

       var mglobal = require('mg-dbx').mglobal;
       var mcursor = require('mg-dbx').mcursor;
       var mclass = require('mg-dbx').mclass;

### Create a Server Object

       var db = new dbx();


### Open a connection to the database

In the following examples, modify all paths (and any user names and passwords) to match those of your own installation.

#### InterSystems Cache

##### API based connectivity

Assuming Cache is installed under **/opt/cache20181/**

           var open = db.open({
               type: "Cache",
               path:"/opt/cache20181/mgr",
               username: "_SYSTEM",
               password: "SYS",
               namespace: "USER"
             });

##### Network based connectivity

Assuming Cache is accessed via **localhost** listening on TCP port **7041**

           var open = db.open({
               type: "Cache",
               host: "localhost",
               tcp_port: 7041,
               username: "_SYSTEM",
               password: "SYS",
               namespace: "USER"
             });


#### InterSystems IRIS

##### API based connectivity

Assuming IRIS is installed under **/opt/IRIS20181/**

           var open = db.open({
               type: "IRIS",
               path:"/opt/IRIS20181/mgr",
               username: "_SYSTEM",
               password: "SYS",
               namespace: "USER"
             });

##### Network based connectivity

Assuming IRIS is accessed via **localhost** listening on TCP port **7041**

           var open = db.open({
               type: "IRIS",
               host: "localhost",
               tcp_port: 7041,
               username: "_SYSTEM",
               password: "SYS",
               namespace: "USER"
             });

#### YottaDB

##### API based connectivity

Assuming an 'out of the box' YottaDB installation under **/usr/local/lib/yottadb/r130**.

           var envvars = "";
           envvars = envvars + "ydb_dir=/root/.yottadb\n"
           envvars = envvars + "ydb_rel=r1.30_x86_64\n"
           envvars = envvars + "ydb_gbldir=/root/.yottadb/r1.30_x86_64/g/yottadb.gld\n"
           envvars = envvars + "ydb_routines=/root/.yottadb/r1.30_x86_64/o*(/root/.yottadb/r1.30_x86_64/r /root/.yottadb/r) /usr/local/lib/yottadb/r130/libyottadbutil.so\n"
           envvars = envvars + "ydb_ci=/usr/local/lib/yottadb/r130/zmgsi.ci\n"
           envvars = envvars + "\n"

           var open = db.open({
               type: "YottaDB",
               path: "/usr/local/lib/yottadb/r130",
               env_vars: envvars
             });

##### Network based connectivity

Assuming YottaDB is accessed via **localhost** listening on TCP port **7041**

           var open = db.open({
               type: "YottaDB",
               host: "localhost",
               tcp_port: 7041,
             });


#### Additional (optional) properties for the open() method

* **multithreaded**: A boolean value to be set to 'true' or 'false' (default: **multithreaded: true**).  Set this property to 'true' if the application uses multithreaded techniques in JavaScript (e.g. V8 worker threads).

* **timeout**: The timeout (in seconds) to be applied to database operations invoked via network based connections.  The default value is 10 seconds.

* **dberror_exceptions**: A boolean value to be set to 'true' or 'false' (default: **dberror_exceptions: false**).  Set this property to 'true' to instruct **mg\-dbx** to throw Node.js exceptions if synchronous invocation of database operations result in an error condition.  If this property is not set, any error condition resulting from the previous database operation can be retrieved using the **db.geterrormessage()** method.


### Return the version of mg-dbx

       var result = db.version();

Example:

       console.log("\nmg-dbx Version: " + db.version());


### Returning (and optionally changing) the current directory (or Namespace)

       current_namespace = db.namespace([<new_namespace>]);

Example 1 (Get the current Namespace): 

       var nspace = db.namespace();

* Note this will return the current Namespace for InterSystems databases and the value of the current global directory for YottaDB (i.e. $ZG).

Example 2 (Change the current Namespace): 

       var new_nspace = db.namespace("SAMPLES");

* If the operation is successful this method will echo back the new Namespace name.  If not successful, the method will return the name of the current (unchanged) Namespace.


### Returning (and optionally changing) the current character set

UTF-8 is the default character encoding for **mg-dbx**.  The other option is the 8-bit ASCII character set (characters of the range ASCII 0 to ASCII 255).  The ASCII character set is a better option when exchanging single-byte binary data with the database.

       current_charset = db.charset([<new_charset>]);

Example 1 (Get the current character set): 

       var charset = db.charset();

Example 2 (Change the current character set): 

       var new_charset = db.charset('ascii');

* If the operation is successful this method will echo back the new character set name.  If not successful, the method will return the name of the current (unchanged) character set.
* Currently supported character sets and encoding schemes: 'ascii' and 'utf-8'.


### Setting (or resetting) the timeout for the connection

       new_timeout = db.settimeout(<new_timeout>);

Specify a new timeout value (in seconds) for the connection.  If the operation is successful this method will return the new value for the timeout.

Example (Set the timeout to 30 seconds): 

       var new_timeout = db.settimeout(30);


### Get the error message associated with the previous database operation

       error_message = db.geterrormessage();

This method will return the error message (as a string) associated with the previous database operation.  An empty string will be returned if the previous operation completed successfully.


### Close database connection

       db.close();
 

## <a name="DBCommands"></a> Invocation of database commands

### Register a global name (and fixed key)


       global = new mglobal(db, <global_name>[, <fixed_key>]);
Or:

       global = db.mglobal(<global_name>[, <fixed_key>]);

Example (using a global named "Person"):

       var person = db.mglobal("Person");

### Set a record

Synchronous:

       var result = <global>.set(<key>, <data>);

Asynchronous:

       <global>.set(<key>, <data>, callback(<error>, <result>));
      
Example:

       person.set(1, "John Smith");

### Get a record

Synchronous:

       var result = <global>.get(<key>);

Asynchronous:

       <global>.get(<key>, callback(<error>, <result>));
      
Example:

       var name = person.get(1);

* Note: use **get\_bx** to receive the result as a Node.js Buffer.

### Delete a record

Synchronous:

       var result = <global>.delete(<key>);

Asynchronous:

       <global>.delete(<key>, callback(<error>, <result>));
      
Example:

       var name = person.delete(1);


### Check whether a record is defined

Synchronous:

       var result = <global>.defined(<key>);

Asynchronous:

       <global>.defined(<key>, callback(<error>, <result>));
      
Example:

       var name = person.defined(1);


### Parse a set of records (in order)

Synchronous:

       var result = <global>.next(<key>);

Asynchronous:

       <global>.next(<key>, callback(<error>, <result>));
      
Example:

       var key = "";
       while ((key = person.next(key)) != "") {
          console.log("\nPerson: " + key + ' : ' + person.get(key));
       }


### Parse a set of records (in reverse order)

Synchronous:

       var result = <global>.previous(<key>);

Asynchronous:

       <global>.previous(<key>, callback(<error>, <result>));
      
Example:

       var key = "";
       while ((key = person.previous(key)) != "") {
          console.log("\nPerson: " + key + ' : ' + person.get(key));
       }


### Increment the value of a global node

Synchronous:

       var result = <global>.increment(<key>, <increment_value>);

Asynchronous:

       <global>.increment(<key>, <increment_value>, callback(<error>, <result>));
      
Example (increment the value of the "counter" node by 1.5 and return the new value):

       var result = person.increment("counter", 1.5);


### Lock a global node

Synchronous:

       var result = <global>.lock(<key>, <timeout>);

Asynchronous:

       <global>.lock(<key>, <timeout>, callback(<error>, <result>));
      
Example (lock global node '1' with a timeout of 30 seconds):

       var result = person.lock(1, 30);

* Note: Specify the timeout value as '-1' for no timeout (i.e. wait until the global node becomes available to lock).


### Unlock a (previously locked) global node

Synchronous:

       var result = <global>.unlock(<key>);

Asynchronous:

       <global>.unlock(<key>, callback(<error>, <result>));
      
Example (unlock global node '1'):

       var result = person.unlock(1);


### Merge (or copy) part of one global to another

* Note: In order to use the 'Merge' facility with YottaDB the M support routines should be installed (**%zmgsi** and **%zmgsis**).

Synchronous (merge from global2 to global1):

       var result = <global1>.merge([<key1>,] <global2> [, <key2>]);

Asynchronous (merge from global2 to global1):

       <global1>.defined([<key1>,] <global2> [, <key2>], callback(<error>, <result>));
      
Example 1 (merge ^MyGlobal2 to ^MyGlobal1):

       global1 = new mglobal(db, 'MyGlobal1');
       global2 = new mglobal(db, 'MyGlobal2');
       global1.merge(global2);

Example 2 (merge ^MyGlobal2(0) to ^MyGlobal1(1)):

       global1 = new mglobal(db, 'MyGlobal1', 1);
       global2 = new mglobal(db, 'MyGlobal2', 0);
       global1.merge(global2);

Alternatively:

       global1 = new mglobal(db, 'MyGlobal1');
       global2 = new mglobal(db, 'MyGlobal2');
       global1.merge(1, global2, 0);

### Reset a global name (and fixed key)

       <global>.reset(<global_name>[, <fixed_key>]);

Example:

       // Process orders for customer #1
       customer_orders = db.mglobal("Customer", 1, "orders")
       do_work ...

       // Process orders for customer #2
       customer_orders.reset("Customer", 2, "orders");
       do_work ...

 
## <a name="Cursors"></a> Cursor based data retrieval

This facility provides high-performance techniques for traversing records held in database globals. 

### Specifying the query

The first task is to specify the 'query' for the global traverse.

       query = new mcursor(db, {global: <global_name>, key: [<seed_key>]}[, <options>]);
Or:

       query = db.mglobalquery({global: <global_name>, key: [<seed_key>]}[, <options>]);

The 'options' object can contain the following properties:

* **multilevel**: A boolean value (default: **multilevel: false**). Set to 'true' to return all descendant nodes from the specified 'seed_key'.

* **getdata**: A boolean value (default: **getdata: false**). Set to 'true' to return any data values associated with each global node returned.

* **format**: Format for output (default: not specified). If the output consists of multiple data elements, the return value (by default) is a JavaScript object made up of a 'key' array and an associated 'data' value.  Set to "url" to return such data as a single URL escaped string including all key values ('key[1->n]') and any associated 'data' value.

Example (return all keys and names from the 'Person' global):

       query = db.mglobalquery({global: "Person", key: [""]}, {multilevel: false, getdata: true});

### Traversing the dataset

In key order:

       result = query.next();

In reverse key order:

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
 

### Traversing the global directory (return a list of global names)

       query = db.mglobalquery({global: <seed_global_name>}, {globaldirectory: true});

Example (return all global names held in the current directory)

       query = db.mglobalquery({global: ""}, {globaldirectory: true});
       while ((result = query.next()) !== null) {
          console.log("result: " + result);
       }


## <a name="DBFunctions"></a> Invocation of database functions

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


* Note: use **function\_bx** to receive the result as a Node.js Buffer.


## <a name="TProcessing"></a> Transaction Processing

M DB Servers implement Transaction Processing by means of the methods described in this section.  When implementing transactions, care should be taken with JavaScript operations that are invoked asynchronously.  All the Transaction Processing methods describe here can only be invoked synchronously.  

* With YottaDB, these methods are only available over network based connectivity to the DB Server.

### Start a Transaction

       result = db.tstart(<parameters>);

* At this time, this method does not take any arguments.
* On successful completion this method will return zero, or an error code on failure.

Example:

       result = db.tstart();


### Determine the Transaction Level

       result = db.tlevel(<parameters>);

* At this time, this method does not take any arguments.
* Transactions can be nested and this method will return the level of nesting.  If no Transaction is active this method will return zero.  Otherwise a positive integer will be returned to represent the current depth of Transaction nesting.

Example:

       tlevel = db.tlevel();


### Commit a Transaction

       result = db.tcommit(<parameters>);

* At this time, this method does not take any arguments.
* On successful completion this method will return zero, or an error code on failure.

Example:

       result = db.tcommit();


### Rollback a Transaction

       result = db.trollback(<parameters>);

* At this time, this method does not take any arguments.
* On successful completion this method will return zero, or an error code on failure.

Example:

       result = db.trollback();


## <a name="DBClasses"></a> Direct access to InterSystems classes (IRIS and Cache)

### Invocation of a ClassMethod

Synchronous:

       result = new mclass(db, <class_name>, <classmethod_name>, <parameters>);
Or:

       result = db.classmethod(<class_name>, <classmethod_name>, <parameters>);

Asynchronous:

       db.classmethod(<class_name>, <classmethod_name>, <parameters>, callback(<error>, <result>));
      
Example (Encode a date to internal storage format):

       result = db.classmethod("%Library.Date", "DisplayToLogical", "10/10/2019");

* Note: use **classmethod\_bx** to receive the result as a Node.js Buffer.


### Creating and manipulating instances of objects

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

### Create an entry for a new Person

       person = db.classmethod("User.Person", "%New");

Add Data:

       result = person.setproperty("Number", 1);
       result = person.setproperty("Name", "John Smith");
       result = person.setproperty("DateOfBirth", "12/8/1995");

Save the object record:

       result = person.method("%Save");

### Retrieve an entry for an existing Person

Retrieve data for object %Id of 1.
 
       person = db.classmethod("User.Person", "%OpenId", 1);

Return properties:

       var number = person.getproperty("Number");
       var name = person.getproperty("Name");
       var dob = person.getproperty("DateOfBirth");

Calculate person's age at a particular date:

       today = db.classmethod("%Library.Date", "DisplayToLogical", "10/10/2019");
       var age = person.method("Age", today);

* Note: use **classmethod\_bx**, **method\_bx** and **getproperty\_bx** to receive data as a Node.js Buffer.

### Reusing an object container

Once created, it is possible to reuse containers holding previously instantiated objects using the **reset()** method.  Using this technique helps to reduce memory usage in the Node.js environment.

Example 1 Reset a container to hold a new instance:

       person.reset("User.Person", "%New");

Example 2 Reset a container to hold an existing instance (object %Id of 2):

       person.reset("User.Person", "%OpenId", 2);


## <a name="DBSQL"></a> Direct access to SQL: MGSQL and InterSystems SQL (IRIS and Cache)

**mg-dbx** provides direct access to the Open Source MGSQL engine ([https://github.com/chrisemunt/mgsql](https://github.com/chrisemunt/mgsql)) and InterSystems SQL (IRIS and Cache).

* Note: In order to use this facility the M support routines should be installed (**%zmgsi** and **%zmgsis**).

### Specifying the SQL query

The first task is to specify the SQL query.

       query = new mcursor(db, {sql: <sql_statement>[, type: <sql_engine>]);
Or:

       query = db.sql({sql: <sql_statement>[, type: <sql_engine>]);

Example 1 (using MGSQL):

       query = db.sql({sql: "select * from person"});


Example 2 (using InterSystems SQL):

       query = db.sql({sql: "select * from SQLUser.person", type: "Cache"});


### Execute an SQL query

Synchronous:

       var result = <query>.execute();

Asynchronous:

       <query>.execute(callback(<error>, <result>));


The result of query execution is an object containing the return code and state and any associated error message.  The familiar ODBC return and status codes are used.

Example 1 (successful execution):

       {
           "sqlcode": 0,
           "sqlstate": "00000",
           "columns": [
                         {
                            "name": "Number",
                            "type": "INTEGER"
                         },
                           "name": "Name",
                            "type": "VARCHAR"
                         },
                           "name": "DateOfBirth",
                            "type": "DATE"
                         }
                      ]
       }


Example 2 (unsuccessful execution):

       {
           "sqlcode": -1,
           "sqlstate": "HY000",
           "error": "no such table 'person'"
       }


### Traversing the returned dataset (SQL 'select' queries)

In result-set order:

       result = query.next();

In reverse result-set order:

       result = query.previous();

In all cases these methods will return 'null' when the end of the dataset is reached.

Example:

       while ((row = query.next()) !== null) {
          console.log("row: " + JSON.stringify(result, null, '\t'));
       }

The output for each iteration is a row of the generated SQL result-set.  For example:

       {
           "number": 1,
           "name": "John Smith",
       }

### SQL cleanup

For 'select' queries that generate a result-set it is good practice to invoke the 'cleanup' method at the end to delete the result-set held in the database.

Synchronous:

       var result = <query>.cleanup();

Asynchronous:

       <query>.cleanup(callback(<error>, <result>));

### Reset an SQL container with a new SQL Query

Synchronous:

       <query>.reset({sql: <sql_statement>[, type: <sql_engine>]);

Asynchronous:

       <query>.reset({sql: <sql_statement>[, type: <sql_engine>], callback(<error>, <result>));


## <a name="Binary"></a> Working with binary data

In **mg-dbx** the default character encoding scheme is UTF-8.  When transmitting binary data between the database and Node.js there are two options.

* Switch to using the 8-bit ASCII character set.
* Receive the incoming data into Node.js Buffers.

On the input (to the database) side all **mg-dbx** function arguments can be presented as Node.js Buffers and **mg-dbx** will automatically detect that an argument is a Buffer and process it accordingly.

On the output side the following functions can be used to return the output as a Node.js Buffer.

* dbx::function\_bx
* dbx::classmethod\_bx

* mglobal::get\_bx

* mclass::classmethod\_bx
* mclass::method\_bx
* mclass::getproperty\_bx

These functions work the same way as their non '_bx' suffixed counterparts.  The only difference is that they will return data as a Node.js Buffer as opposed to a type of String.

The following two examples illustrate the two schemes for receiving binary data from the database.

Example 1: Receive binary data from a DB function as a Node.js 8-bit character stream

       <db>.charset('ascii');
       var stream_str8 = <db>.function(<function>, <parameters>);
       <db>.charset('utf-8'); // reset character encoding

Example 2: Receive binary data from a DB function as a Node.js Buffer

       var stream_buffer = <db>.function_bx(<function>, <parameters>);


## <a name="Threads"></a> Using Node.js/V8 worker threads

**mg-dbx** functionality can now be used with Node.js/V8 worker threads.  This enhancement is available with Node.js v12 (and later).

* Note: be sure to include the property **multithreaded: true** in the **open** method when opening database  connections to be used in multi-threaded applications.

Use the following constructs for instantiating **mg-dbx** objects in multi-threaded applications:

        // Use:
        var <global> = new mglobal(<db>, <global>);
        // Instead of:
	    var <global> = <db>.mglobal(<global>);

        // Use:
        var <cursor> = new mcursor(<db>, <global_query>);
        // Instead of:
        var <cursor> = <db>.mglobalquery(<global_query>)

        // Use:
        var <class> = new mclass(<db>, <classmethod>);
        // Instead of:
        var <class> = <db>.classmethod(<classmethod>);

        // Use:
        var <sql> = new mcursor(<db>, <sqlquery>);
        // Instead of:
        var <sql> = <db>.sql(<sqlquery>);


The following scheme illustrates how **mg-dbx** should be used in threaded Node.js applications.

       const { Worker, isMainThread, parentPort, threadId } = require('worker_threads');

       if (isMainThread) {
          // start the threads
          const worker1 = new Worker(__filename);
          const worker2 = new Worker(__filename);

          // process messages received from threads
          worker1.on('message', (message) => {
             console.log(message);
          });
          worker2.on('message', (message) => {
             console.log(message);
          });
       } else {
          var dbx = require('mg-dbx').dbx;
          // And as required ...
          var mglobal = require('mg-dbx').mglobal;
          var mcursor = require('mg-dbx').mcursor;
          var mclass = require('mg-dbx').mclass;

          var db = new dbx();
          db.open(<parameters>);

          var global = new mglobal(db, <global>);

          // do some work

          var result = db.close();
          // tell the parent that we're done
          parentPort.postMessage("threadId=" + threadId + " Done");
       }

## <a name="EventLog"></a> The Event Log

**mg\-dbx** provides an Event Log facility for recording errors in a physical file and, as an aid to debugging, recording the **mg\-dbx** functions called by the application.  This Log facility can also be used by Node.js applications.

To use this facility, the Event Log file must be specified using the following function:


       db.setloglevel(<log_file>, <Log_level>, <log_filter>);

Where:

* **log\_file**: The name (and path to) the log file you wish to use. The default is c:/temp/mg-dbx.log (or /tmp/mg-dbx.log under UNIX).
* **log\_level**: A set of characters to include one or more of the following:
	* **e** - Log error conditions.
	* **f** - Log all **mg\-dbx** function calls (function name and arguments).
	* **t** - Log the request data buffers to be transmitted from **mg\-dbx** to the DB Server.
	* **r** - Log the request data buffers to be transmitted from **mg\-dbx\-bdb** to the DB Server and the corresponding response data.
* **log\_filter**: A comma-separated list of functions that you wish the log directive to be active for. This should be left empty to activate the log for all functions.

Examples:

      db.setloglevel("c:/temp/mg-dbx.log", "e", "");
      db.setloglevel("/tmp/mg-dbx.log", "ft", "dbx::set,mglobal::set,mcursor::execute");

Node.js applications can write their own messages to the Event Log using the following function:

      db.logmessage(<message>, <title>);

Logging can be switched off by calling the **setloglevel** function without specifying a log level.  For example:

      db.setloglevel("c:/temp/mg-dbx.log");

## <a name="License"></a> License

Copyright (c) 2018-2021 M/Gateway Developments Ltd,
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

### v1.3.7 (1 November 2019)

* Introduce support for direct access to InterSystems SQL and MGSQL.
* Correct a fault in the InterSystems Cache/IRIS API to globals that resulted in failures - notably in cases where there was a mix of string and numeric data in the global records.

### v1.3.8 (14 November 2019)

* Correct a fault in the Global Increment method.
* Correct a fault that resulted in query.next() and query.previous() loops not terminating properly (with null) under YottaDB.  This fault affected YottaDB releases after 1.30
* Modify the version() method so that it returns the version of YottaDB rather than the version of the underlying GT.M engine.

### v1.3.9 (26 February 2020)

* Verify that **mg-dbx** will build and work with Node.js v13.x.x.
* Suppress a number of benign 'cast-function-type' compiler warnings when building on the Raspberry Pi.

### v1.3.9a (21 April 2020)

* Verify that **mg-dbx** will build and work with Node.js v14.x.x.

### v1.4.10 (6 May 2020)

* Introduce support for Node.js/V8 worker threads (for Node.js v12.x.x. and later).
	* See the section on 'Using Node.js/V8 worker threads'.
* Introduce support for the M Merge command.
* Correct a fault in the mcursor 'Reset' method.

### v1.4.11 (14 May 2020)

* Introduce a scheme for transmitting binary data between Node.js and the database.
* Correct a fault that led to some calls failing with incorrect data types after calls to the **mglobal::increment** method.
* **mg-dbx** will now pass arguments to YottaDB functions as **ydb\_string\_t** types and not **ydb\_char\_t**.  Modify your YottaDB function interface file accordingly.  See the section on 'Installing the M support routines'.

### v2.0.12 (25 May 2020)

* Introduce the option to connect to a database over the network.
* Remove the 32K limit on the volume of data that can be sent to the database via the **mg-dbx** methods.
* Correct a fault that led to the failure of asynchronous calls to the **dbx::function** and **mglobal::previous** methods.

### v2.0.13 (8 June 2020)

* Correct a fault in the processing of InterSystems Object References (orefs).
	* This fault only affected applications using API-based connectivity to the database (as opposed to network-based connectivity).
	* The fault could result in Node.js throwing 'Heap Corruption' errors after creating an instance of an InterSystems Object.

### v2.0.14 (17 June 2020)

* Extend the processing of InterSystems Object References (orefs) to cater for instances of an object embedded as a property in other objects.  For example, consider two classes: Patient and Doctor where an instance of a Doctor may be embedded in a Patient record (On the Server: "Property MyDoctor As Doctor").  And on the Node.js side...

        var patient = db.classmethod("User.Patient", "%OpenId", patient_id);
        var doctor = patient.getproperty("MyDoctor");
        var doctor_name = doctor.getproperty("Name");

* Correct a fault in the processing of output values returned from YottaDB functions that led to output string values not being terminated correctly.  The result being unexpected characters appended to function outputs.

### v2.0.15 (22 June 2020)

* Correct a fault that could lead to fatal error conditions when creating new JS objects in multithreaded Node.js applications (i.e. when using Node.js/V8 worker threads).

### v2.0.16 (8 July 2020)

* Correct a fault that could lead to **mg-dbx** incorrectly reporting _'Database not open'_ errors when connecting to YottaDB via its API in multithreaded Node.js applications.

### v2.1.17 (1 August 2020)

* Introduce a log facility to record error conditions and run-time information to assist with debugging.
* Change the default for the **multihtreaded** property to be **true**.  This can be set to **false** (in the **open()** method) if you are sure that your application does not use Node.js/V8 threading and does not call **mg\-dbx** functionality asynchronously.  If in doubt, it is safer to leave this property set to **true**.
* A number of faults related to the use of **mg\-dbx** functionality in Node.js/v8 worker threads have been corrected.  In particular, it was noticed that callback functions were not being fired correctly for some asynchronous invocations of **mg\-dbx** methods.

### v2.1.18 (12 August 2020)

* Correct a fault that could lead to unpredictable behaviour and failures if more than one V8 worker thread concurrently requested a global directory listing.
	* For example: query = new cursor(db, {global: ""}, {globaldirectory: true});
* For SQL SELECT queries, return the column names and their associated data types.
	* This metadata is presented as a **columns** array within the object returned from the SQL Execute method.
	* The **columns** array is created in SELECT order.
* Attempt to capture Windows OS exceptions in the event log.
	* The default event log is c:\temp\mg-dbx.log under Windows and /tmp/mg-dbx.log under UNIX.

### v2.1.19 (15 August 2020)

* Update the internal UNIX library names for InterSystems IRIS and Cache.
	* For information, the Cache library was renamed from libcache to libisccache and the IRIS library from libirisdb to libiscirisdb
	* This change does not affect Windows platforms.

### v2.1.19a (13 November 2020)

* Verify that **mg-dbx** will build and work with Node.js v15.x.x.
* Note that files that were previously held under the **/m**, **/yottadb**, and **/unix** directories are now available from the **mgsi** GitHub repository.  These files are common to a number of my Open Source projects.
	*  [https://github.com/chrisemunt/mgsi](https://github.com/chrisemunt/mgsi)

### v2.1.20 (9 December 2020)

* Correct a fault that occasionally led to failures in network-based connectivity between **mg\-dbx** and DB Servers.

### v2.2.21 (6 January 2021)

* Allow a DB Server response timeout to be set for network based connectivity.
	* Specify the **timeout** property in the open() method.
	* Use the **db.settimeout()** method to set or reset the timeout value.
* Introduce an option to throw Node.js exceptions if synchronous calls to database operations result in an error condition (for example an M "SUBSCRIPT" or "SYNTAX" error).
	* Specify the **dberror_exceptions** property in the **open()** method (default is **false**). 
* Introduce a method to return any error message associated with the previous database operation.
	* **var errormessage = db.geterrormessage()**

### v2.2.22 (18 January 2021)

* Extend the logging of request transmission data to include the corresponding response data.
	* Include 'r' in the log level.  For example: db.setloglevel("MyLog.log", "eftr", "");
* Correct a fault that occasionally led to failures when sending long strings (greater than 32K) to the DB Server.
	* For example global.set('key1', 'key2', [string 2MB in length]);
* Correct a fault that occasionally led to failures when returning long strings to Node.js from the DB Server.
	* This fault only affected network based connectivity to the DB Server.  

### v2.3.23 (12 February 2021)

* Introduce support for M Transaction Processing: tstart, $tlevel, tcommit, trollback.

