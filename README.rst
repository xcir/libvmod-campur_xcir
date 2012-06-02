===================
vmod_campur_xcir
===================

-------------------------------
Varnish campur module
-------------------------------

:Author: Syohei Tanaka(@xcir)
:Date: 2012-02-13
:Version: 0.2
:Manual section: 3

SYNOPSIS
===========

import campur_xcir;

DESCRIPTION
==============


FUNCTIONS
============

gethash
-------------

Prototype
        ::

                gethash()
Return value
	STRING hashstring
Description
	get hash string.(call after vcl_hash)
Example
        ::

                set resp.http.X-HASH=campur_xcir.gethash();

                //response
                X-HASH: c6643369e6d89365c552af9c2fa153db5c24fd8cb0f58e48125e01b433ac3677
postcapture
-------------

Prototype
        ::

                postcapture(STRING target)
Return value
	INT
Description
	(experimental:Might change the parameter)
	get POST request(Only "application/x-www-form-urlencoded")
Example
        ::

                if(campur_xcir.postcapture("x-test") == 1){
                  std.log("raw: " + req.http.x-test);
                  std.log("submitter: " + req.http.submitter);
                }

                //response
                12 VCL_Log      c raw: submitter=abcdef&submitter2=b
                12 VCL_Log      c submitter: abcdef


inet_pton
-------------

Prototype
        ::

                inet_pton(BOOL ipv6 , STRING str , STRING defaultstr)
Return value
	IP
Description
	Convert IP addresses from string
Example
        ::

                set resp.http.v6 = campur_xcir.inet_pton(true,"2001:0db8:bd05:01d2:288a:1fc0:0001:10ee","1982:db8:20:3:1000:100:20:3");
                set resp.http.v4 = campur_xcir.inet_pton(false,"1.1.1.1","2.2.2.2");
                set resp.http.v6ng = campur_xcir.inet_pton(true,"2001:0db8:bd05:01d2:288a:1fc0:0001:10eeHOGE","1982:db8:20:3:1000:100:20:3");
                set resp.http.v4ng = campur_xcir.inet_pton(false,"1.1.1.1HOGE","2.2.2.2");

                //response
                v6: 2001:db8:bd05:1d2:288a:1fc0:1:10ee
                v4: 1.1.1.1
                v6ng: 1982:db8:20:3:1000:100:20:3
                v4ng: 2.2.2.2
                
                
                //acl
                acl local {
                    "192.168.1.0"/24;
                    !"0.0.0.0";
                }
                if(campur_xcir.inet_pton(false , regsub(req.http.X-Forwarded-For,"^([0-9]+.[0-9]+.[0-9]+.[0-9]+).*$" , "\1") , "0.0.0.0") ~ local){
                    //acl ok
                    ...
                }

timecmp
-------------

Prototype
        ::

                timecmp(TIME time1 , TIME time2)
Return value
	DURATION
Description
	return(time1-time2)

timeoffset
-------------

Prototype
        ::

                timeoffset(TIME time , DURATION os , BOOL rev)
Return value
	TIME
Description
	Calculate time

Example
        ::

                set resp.http.x = campur_xcir.timeoffset(now , 1d , false);
                set resp.http.y = campur_xcir.timeoffset(now , 1d , true);
                set resp.http.z = now;
                
                //response
                x: Fri, 13 Apr 2012 16:15:40 GMT
                y: Wed, 11 Apr 2012 16:15:40 GMT
                z: Thu, 12 Apr 2012 16:15:40 GMT

INSTALLATION
==================

Installation requires Varnish source tree.

Usage::

 ./autogen.sh
 ./configure VARNISHSRC=DIR [VMODDIR=DIR]

`VARNISHSRC` is the directory of the Varnish source tree for which to
compile your vmod. Both the `VARNISHSRC` and `VARNISHSRC/include`
will be added to the include search paths for your module.

Optionally you can also set the vmod install directory by adding
`VMODDIR=DIR` (defaults to the pkg-config discovered directory from your
Varnish installation).

Make targets:

* make - builds the vmod
* make install - installs your vmod in `VMODDIR`
* make check - runs the unit tests in ``src/tests/*.vtc``


HISTORY
===========

Version 0.3: add function postcapture
Version 0.2: add function timecmp , inet_pton , timeoffset
Version 0.1: add function gethash

COPYRIGHT
=============

This document is licensed under the same license as the
libvmod-rewrite project. See LICENSE for details.

* Copyright (c) 2012 Syohei Tanaka(@xcir)

File layout and configuration based on libvmod-example

* Copyright (c) 2011 Varnish Software AS

postcapture method based on VFW( https://github.com/scarpellini/VFW )
