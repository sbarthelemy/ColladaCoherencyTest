/*
The MIT License

Copyright 2006 Sony Computer Entertainment Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef _COHERENCYTEST_H_
#define _COHERENCYTEST_H_

#ifdef WIN32
#include "Windows.h"
#endif 

#include "libxml/xmlschemas.h"
#include "libxml/schemasInternals.h"
#include "libxml/schematron.h"
#include "libxml/xmlreader.h"

#include <map>
#include <set>
#include <string>
#include <vector>
#include <time.h>
using namespace std;

#ifndef MAX_PATH
#define MAX_PATH 1024
#endif
#define MAX_LOG_BUFFER 1024
#define MAX_NAME_SIZE 512 


#include "dae.h" 
#include "dae/daeSIDResolver.h"
#include "dae/daeErrorHandler.h"
#include "dom/domTypes.h"
#include "dom/domCOLLADA.h"
#include "dom/domConstants.h"
#include "dom/domElements.h"
#include "dom/domProfile_GLES.h"
#include "dom/domProfile_GLSL.h"
#include "dom/domProfile_CG.h"
#include "dom/domProfile_COMMON.h"
#include "dom/domFx_include_common.h"


#ifndef stricmp
inline int stricmp(const char *s1, const char *s2)
{
  char f, l;

  do 
  {
    f = ((*s1 <= 'Z') && (*s1 >= 'A')) ? *s1 + 'a' - 'A' : *s1;
    l = ((*s2 <= 'Z') && (*s2 >= 'A')) ? *s2 + 'a' - 'A' : *s2;
    s1++;
    s2++;
  } while ((f) && (f == l));

  return (int) (f - l);
}

#endif 

class CoherencyTestErrorHandler : public daeErrorHandler
{
public:
	CoherencyTestErrorHandler();
	virtual ~CoherencyTestErrorHandler();

public:
	void handleError( daeString msg );
	void handleWarning( daeString msg );
};

#endif //_COHERENCYTEST_H_

