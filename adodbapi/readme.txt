Project
-------
adodbapi -- win32 inclusion version

A Python DB-API 2.0 module that makes it easy to use Microsoft ADO 
for connecting with databases and other data sources.

Home page: <http://sourceforge.net/projects/adodbapi>

Features:
* 100% DB-API 2.0 compliant. 
* Includes pyunit testcases that describes how to use the module.  
* Fully implemented in Python. 
* Licensed under the LGPL license, which means that it can be used freely even in commercial programs subject to certain restrictions. 
* Supports eGenix mxDateTime, Python 2.3 datetime module and Python time module.

Prerequisites:
* Python 2.3 or higher. 
* (this version included within) Mark Hammond's win32all python for windows extensions. 

Whats new in version 2.1?
1. Use of Decimal.decimal data type for currency and numeric data. [ Cole ]
2. add optional timeout parameter to the connect method i.e.: 
      adodbapi.connect(string,timeout=nuberOfSeconds)  [thanks to: Patrik Simons]
3. improved detection of underlying transaction support [ Thomas Albrecht ]
4. fixed record set closing bug and adoRowIDtype [ Erik Rose ]
5. client-side cursoring [ Erik Rose ]
6. correct string length for qmark parameter substitution [ Jevon a.k.a. Guybrush Threepwood ]
7. try-multiple-strategy loop replaced by other logic. [ Cole ]
8. correct error code raised when next recordset not available. [ ekelund ]
9. numerous changes in unit test (including test against mySQL)

Whats new in version 2.0.1?
Added missing __init__.py file so it installs correctly.

Whats new in version 2.0?
1. Improved performance through GetRows method.
2. Flexible date conversions. 
   Supports eGenix mxExtensions, python time module and python 2.3 datetime module.
3. More exact mappings of numeric datatypes. 
4. User defined conversions of datatypes through "plug-ins".
5. Improved testcases, not dependent on Northwind data.
6. Works with Microsoft Access currency datatype.
7. Improved DB-API 2.0 compliance.
8. rowcount property works on statements Not returning records.
9. "Optional" error handling extension, see DB-API 2.0 specification.

Installation
------------
This version will be installed as part of the win32 package.

Authors (up to version 2.0.1)
-------
Henrik Ekelund, 
Jim Abrams.
Bjorn Pettersen.

Authors (version 2.1)
-------
Vernon Cole

License
-------
LGPL, see http://www.opensource.org/licenses/lgpl-license.php


Documentation
-------------
Start with:
http://www.python.org/topics/database/DatabaseAPI-2.0.html
read adodbapi/test/db_print.py
and look at the test cases in adodbapi/test directory. 

Mailing lists
-------------
The adodbapi mailing lists have been deactivated. Submit comments to the 
bug tracker.

Contribute
----------
Use the sourceforge bug tracking system to submit bugs, feature requests
and comments.

Relase history
--------------
2.1	Python 2.4 version
2.0     See what's new above.
1.0.1   Bug fix: Null values for numeric fields. Thanks to Tim Golden. 
1.0     the first release
