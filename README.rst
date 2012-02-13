============
vmod_campur_xcir
============

----------------------
Varnish campur module
----------------------

:Author: Syohei Tanaka(@xcir)
:Date: 2012-02-13
:Version: 0.1
:Manual section: 3

SYNOPSIS
========

import campur_xcir;

DESCRIPTION
===========


FUNCTIONS
=========

gethash
---------

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

INSTALLATION
============

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

In your VCL you could then use this vmod along the following lines::
        
        import rewrite;

        sub vcl_recv {
                //redirect to my hp
                error(rewrite.location(302,"http://xcir.net/"));
        }

HISTORY
=======

Version 0.1: add function gethash

COPYRIGHT
=========

This document is licensed under the same license as the
libvmod-rewrite project. See LICENSE for details.

* Copyright (c) 2012 Syohei Tanaka(@xcir)

File layout and configuration based on libvmod-example

* Copyright (c) 2011 Varnish Software AS
