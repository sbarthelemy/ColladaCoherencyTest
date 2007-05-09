/*
 * Copyright 2006 Sony Computer Entertainment Inc.
 *
 * Licensed under the SCEA Shared Source License, Version 1.0 (the "License"); you may not use this 
 * file except in compliance with the License. You may obtain a copy of the License at:
 * http://research.scea.com/scea_shared_source_license.html
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License 
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or 
 * implied. See the License for the specific language governing permissions and limitations under the 
 * License. 
 */

/*
List of Checks
Check_links		It checks if all id are valid reference and if we can resolveElement and getElement 
				from a link    
Check_unique_id	It checks if all Ids in each document are unique     
Check_counts	It checks number counts are correctly set, eg. 
				skin vertex count should be = mesh vertex count     
				accessor has the right count on arrays from stride and counts.
Check_files		It checks if the image files, cg/fx files, and other non-dae files that the document 
				referenced exist     
Check_textures  It checks if the textures are correctly defined/used (image, surface, sampler, 
				instancing...)
				<texture> shouldn't directly reference to <image> id.
				it should reference <newparam>'s sid, and have <sampler2D> with <source> reference to
				another <newparam>'s sid that have <surface> with <init_from> refernce to <image> id.

Check_URI		It checks if the URI are correct. It should also check for unescaped spaces because 
				a xml validator won't catch the problem. Reference http://www.w3.org/TR/xmlschema-2/#anyURI
Check_schema	It checks if the document validates against the Schema   
Check_inputs	It checks if the required number of input elements are present and that they have the 
				correct semantic values for their sources.   
Check_skin		It will check if values in name_array should only reference to an existing SID, 
				and values in IDREF_array should only reference to an existing ID  
Check_InstanceGeometry It checks if all Intance_geometry has bind_material that has a correct matching 
				bind_material between symbol and target
Check_Controller It checks if skin have same number of vertices weight as the vertices number of geometry.
                It checks if morph have same number of vertices from source geometry as number of vertices 
				in all other target geometry.
Check_Float_array It checks if NaN, INF, -INF exist in all the float array        
*/

#include "Windows.h"

#include "libxml/xmlschemas.h"
#include "libxml/schemasInternals.h"
#include "libxml/schematron.h"
#include "libxml/xmlreader.h"
#include "iconv.h"

#include <map>
#include <set>
#include <string>
#include <vector>
using namespace std;

#ifndef MAX_PATH
#define MAX_PATH 1024
#endif
#define MAX_LOG_BUFFER 1024
#define MAX_NAME_SIZE 512 


#include "dae.h" 
#include "dae/daeSIDResolver.h"
#include "dom/domTypes.h"
#include "dom/domCOLLADA.h"
#include "dom/domConstants.h"
#include "dom/domElements.h"
#include "dom/domProfile_GLES.h"
#include "dom/domProfile_COMMON.h"
#include "dom/domFx_include_common.h"


//Coherencytest * globalpointer;
//#define PRINTF if(VERBOSE) printf

string file_name, log_file;
string output_file_name = "";
FILE * file;
FILE * log;
bool quiet;

void PRINTF(const char * str)
{
	if (str==0) return;
	if (quiet == false)
		printf("%s",str);
	if (file)
		fwrite(str, sizeof(char), strlen(str), file);
	if (log)
		fwrite(str, sizeof(char), strlen(str), log);
}

int VERBOSE;
void print_name_id(domElement * element);
domUint CHECK_error(domElement * element, bool b,  const char * message= NULL);
void CHECK_warning(domElement * element, bool b, const char *message = NULL);
domUint CHECK_uri(xsAnyURI & uri);
domUint CHECK_count(domElement * element, domInt expected, domInt result, const char *message = NULL);
bool CHECK_fileexist(const char * filename);
domUint CHECK_file(domElement *element, xsAnyURI & fileuri);
domUint	GetMaxOffsetFromInputs(domInputLocalOffset_Array & inputs);
domUint CHECK_Triangles(domTriangles *triangles);
domUint CHECK_Polygons(domPolygons *polygons);
domUint CHECK_Polylists(domPolylist *polylist);
domUint CHECK_Tristrips(domTristrips *tristrips);
domUint CHECK_Trifans(domTrifans *trifans);
domUint CHECK_Lines(domLines *lines);
domUint CHECK_Linestrips(domLinestrips *linestrips);
domUint CHECK_Geometry(domGeometry *geometry);
domUint CHECK_InstanceGeometry(domInstance_geometry * instance_geometry);
domUint CHECK_Controller(domController *controller);
domUint CHECK_InstanceElementUrl(daeDatabase *db, const char * instance_element);
domUint GetSizeFromType(xsNMTOKEN type);
domUint CHECK_Source(domSource * source);
//	void _XMLSchemaValidityErrorFunc(void* ctx, const char* msg, ...);
//	void _XMLSchemaValidityWarningFunc(void* ctx, const char* msg, ...);
domUint CHECK_validateDocument(_xmlDoc *LXMLDoc);
domUint CHECK_xmlfile(daeString filename);

domUint CHECK_counts (DAE *input, int verbose = 0);
domUint CHECK_links (DAE *input, int verbose = 0);
domUint CHECK_files (DAE *input, int verbose = 0);
domUint CHECK_unique_id (DAE *input, int verbose = 0);
domUint CHECK_texture (DAE *input, int verbose = 0);
domUint CHECK_schema (DAE *input, int verbose = 0);
domUint CHECK_skin (DAE *input, int verbose = 0);
domUint CHECK_inputs (domInputLocal_Array & inputs, const char * semantic);
domUint CHECK_inputs (domInputLocalOffset_Array & inputs, const char * semantic);
domUint CHECK_float_array (DAE *input, int verbose = 0);
domUint CHECK_Circular_Reference (DAE *input, int verbose = 0);
domUint CHECK_Index_Range (domElement * elem, domListOfUInts & listofint, domUint index_range, domUint offset, domUint maxoffset, int verbose = 0);
domUint CHECK_Index_Range (domElement * elem, domListOfInts & listofint, domUint index_range, domUint offset, domUint maxoffset, int verbose = 0);


const char VERSION[] = 
"Coherencytest version 1.0\n"
"Licensed under the SCEA Shared Source License, Version 1.0 (the \"License\"); you may not use this\n" 
"file except in compliance with the License. You may obtain a copy of the License at:\n"
"http://research.scea.com/scea_shared_source_license.html\n";

const char USAGE[] =
"Usage: coherencytest filename.dae ... [OPTION]...\n"
" option:                           \n"
" -log filename.log         - log warnings and errors in filename.log          \n"
" filename.dae				- check collada file filename.dae, filename.dae should be a url format\n"
" -check SCHEMA COUNTS ..   - check SCHEMA and COUNTS only, test all if not specify any\n"
"                             available checks:              \n"
"                             SCHEMA						\n"
"                             UNIQUE_ID 					\n"
"                             COUNTS						\n"
"                             LINKS							\n"
"                             TEXTURE						\n"
"                             FILES							\n"
"                             SKIN							\n"
"                             FLOAT_ARRAY					\n"
"                             CIRCULR_REFERENCE				\n"
"                             INDEX_RANGE					\n"
" -ignore SCHEMA COUNTS ..  - ignore SCHEMA and COUNTS only, test all if not specify any\n"
" -quiet -q                 - disable printfs and MessageBox\n"
" -version                  - print version and copyright information\n";

std::map<string, bool> checklist;
int main(int Argc, char **argv)
{
	std::vector<string> file_list;
	int err = 0;
	log = 0;
	bool checkall = true;
	domUint errorcount = 0;
	quiet = false;

	for (int i=1; i<Argc; i++)
	{	
		if (stricmp(argv[i], "-log") == 0)
		{
			i++;
			if (i <= Argc)
				log_file = argv[i];
			log = fopen(log_file.c_str(), "w+");
		} else if (stricmp(argv[i], "-check") == 0)
		{
			i++;
			if (i >= Argc)
				break;
			while (argv[i][0]!='-')
			{
				checkall = false;
				for (size_t j=0; j<strlen(argv[i]); j++)
					argv[i][j] = toupper(argv[i][j]);
				checklist[argv[i]] = true;
				i++;
				if (i >= Argc)
					break;
			}
			i--;
		} else if (stricmp(argv[i], "-ignore") == 0)
		{
			checkall = false;
			checklist["SCHEMA"]				= true;
			checklist["UNIQUE_ID"]			= true;
			checklist["COUNTS"]				= true;
			checklist["LINKS"]				= true;
			checklist["TEXTURE"]			= true;
			checklist["FILES"]				= true;
			checklist["SKIN"]				= true;
			checklist["FLOAT_ARRAY"]		= true;
			checklist["CIRCULR_REFERENCE"]	= true;
			checklist["INDEX_RANGE"]		= true;
			i++;
			if (i >= Argc)
				break;
			while (argv[i][0]!='-')
			{
				for (size_t j=0; j<strlen(argv[i]); j++)
					argv[i][j] = toupper(argv[i][j]);
				checklist[argv[i]] = false;
				i++;
				if (i >= Argc)
					break;
			}
			i--;
		} else if (stricmp(argv[i], "-version") == 0)
		{
			printf(VERSION);
			return 0;
		} else if (stricmp(argv[i], "-quiet") == 0 || stricmp(argv[i], "-q") == 0)
		{
			quiet = true;
		} else 
		{
			file_list.push_back(argv[i]);
		}
	}
	if (file_list.size() == 0)
	{
		printf(USAGE);
		return 0;
	}
	
	for (size_t i=0; i<file_list.size(); i++)
	{
		file_name = file_list[i];
		output_file_name = file_name + string(".log");

		if (isalpha(file_name[0]) && file_name[1]==':' && file_name[2]=='\\')
		{
			file_name = string("/") + file_name;
			for (unsigned int i=0; i<file_name.size(); i++)
			{
				if (file_name[i] =='\\')
					file_name[i] = '/';
			}
		}

		DAE * dae = new DAE;
		err = dae->load(file_name.c_str());
		if (err != 0) {
			if (quiet == false)
			{
				printf("DOM Load error = %d\n", err);
				printf("filename = %s\n", file_name.c_str());
				MessageBox(NULL, "Collada Dom Load Error", "Error", MB_OK);
			}
			return err;
		}

		file = fopen(output_file_name.c_str(), "w+");

		if (checkall)
		{
			checklist["SCHEMA"]				= true;
			checklist["UNIQUE_ID"]			= true;
			checklist["COUNTS"]				= true;
			checklist["LINKS"]				= true;
			checklist["TEXTURE"]			= true;
			checklist["FILES"]				= true;
			checklist["SKIN"]				= true;
			checklist["FLOAT_ARRAY"]		= true;
			checklist["CIRCULR_REFERENCE"]	= true;
			checklist["INDEX_RANGE"]		= true;
		}

		//VERBOSE = verbose;
		if (checklist["SCHEMA"])
			errorcount += CHECK_schema(dae);
		if (checklist["UNIQUE_ID"])
			errorcount += CHECK_unique_id(dae);
		if (checklist["COUNTS"])
			errorcount += CHECK_counts(dae);
		if (checklist["LINKS"])
			errorcount += CHECK_links(dae);
		if (checklist["TEXTURE"])
			errorcount += CHECK_texture(dae);
		if (checklist["FILES"])
			errorcount += CHECK_files(dae);
		if (checklist["SKIN"])
			errorcount += CHECK_skin(dae);
		if (checklist["FLOAT_ARRAY"])
			errorcount += CHECK_float_array(dae);
		if (checklist["CIRCULR_REFERENCE"])
			errorcount += CHECK_Circular_Reference (dae);

		if (file) fclose(file);
		delete dae;
	}
	if (log) fclose(log);
	if (errorcount == 0)
		remove(output_file_name.c_str());
	return (int) errorcount;
}

void print_name_id(domElement * element)
{
	domElement * e = element;
	while ( e->getID() == NULL)
		e = e->getParentElement();
	char temp[MAX_LOG_BUFFER];
	sprintf(temp, "(type=%s,id=%s)", e->getTypeName(), e->getID());
	PRINTF(temp);
}
domUint CHECK_error(domElement * element, bool b,  const char * message)
{
	if (b == false) {
		PRINTF("ERROR: ");
		if (element) print_name_id(element);
		if (message) PRINTF(message);
		return 1;
	}
	return 0;
}
void CHECK_warning(domElement * element, bool b, const char *message)
{
	if (b == false) {
		PRINTF("WARNING: ");
		print_name_id(element);
		if (message) PRINTF(message);
	}
}
domUint CHECK_uri(xsAnyURI & uri)
{
//	uri.resolveElement();
	if (uri.getElement() == NULL)
	{
		char temp[MAX_LOG_BUFFER];
		sprintf(temp, "ERROR: CHECK_uri Failed uri=%s not resolved\n",uri.getURI());
		PRINTF(temp);
		return 1;
	}

	return 0;//CHECK_escape_char(uri.getOriginalURI());
}
domUint CHECK_count(domElement * element, domInt expected, domInt result, const char *message)
{
	if (expected != result)
	{
		char temp[MAX_LOG_BUFFER];
		sprintf(temp, "ERROR: CHECK_count Failed: expected=%d, result=%d\n   ", expected, result);
		PRINTF(temp);
		print_name_id(element);
		if (message) PRINTF(message);
		return 1;
	}
	return 0;
}
bool CHECK_fileexist(const char * filename)
{
	xmlTextReader * reader = xmlReaderForFile(filename, 0, 0);
	if (!reader)
		return false;
	return true;
}
domUint CHECK_file(domElement *element, xsAnyURI & fileuri)
{
	daeURI * uri = element->getDocumentURI();
	daeString TextureFilePrefix = uri->getFilepath();

	// Build a path using the scene name ie: images/scene_Textures/boy.tga
	daeChar newTexName[MAX_NAME_SIZE]; 	
	sprintf(newTexName, "%s%s", TextureFilePrefix, fileuri.getURI() ); 
	
	// Build a path for the Shared texture directory ie: images/Shared/boy.tga
	daeChar sharedTexName[MAX_NAME_SIZE];
	sprintf(sharedTexName, "%sShared/%s",TextureFilePrefix, fileuri.getFile() );

	if (!CHECK_fileexist(fileuri.getURI()))
		if(!CHECK_fileexist(newTexName))
			if(!CHECK_fileexist(sharedTexName))
			{
				char temp[MAX_LOG_BUFFER];
				sprintf(temp, "ERROR: CHECK_file failed, %s not found\n", fileuri.getURI());
				PRINTF(temp);
				return 1;
			}
	return 0;
}

domUint	GetMaxOffsetFromInputs(domInputLocalOffset_Array & inputs)
{
	domUint maxoffset = 0;
	domUint count = (domUint) inputs.getCount();
	for(size_t i=0; i< count ;i++)
	{
		domUint thisoffset  = inputs[i]->getOffset();
		if (maxoffset < thisoffset) maxoffset++;
	}
	return maxoffset + 1;
}

domUint GetIndexRangeFromInput(domInputLocalOffset_Array &input_array, domUint offset)
{
	for (size_t j=0; j<input_array.getCount(); j++)
	{
		if (input_array[j]->getOffset() == offset)
		{
			if (stricmp(input_array[j]->getSemantic(), "VERTEX") == 0)
			{	// vertex
				domVertices * vertices = (domVertices*)(domElement*) input_array[j]->getSource().getElement();
				if (vertices) 
				{
					domInputLocal_Array & inputs = vertices->getInput_array();
					for (size_t i=0; i<inputs.getCount(); i++)
					{
						if (stricmp(inputs[i]->getSemantic(), "POSITION") == 0)
						{
							domSource * source = (domSource*)(domElement*) inputs[i]->getSource().getElement();
							if (source)
							{
								domSource::domTechnique_common * technique_common = source->getTechnique_common();
								if (technique_common)
								{
									domAccessor * accessor = technique_common->getAccessor();
									if (accessor)
										return accessor->getCount();
								}
							}
						}
					}
				}
			} else { // non-vertex
				domSource * source = (domSource*)(domElement*) input_array[j]->getSource().getElement();
				if (source)
				{
					domSource::domTechnique_common * technique_common = source->getTechnique_common();
					if (technique_common)
					{
						domAccessor * accessor = technique_common->getAccessor();
						if (accessor)
							return accessor->getCount();
					}
				}
			}
		}
	}
	return 0;
}
domUint CHECK_Triangles(domTriangles *triangles)
{
	domUint errorcount = 0;
	domUint count = triangles->getCount();
	domInputLocalOffset_Array & inputs = triangles->getInput_array();
	errorcount += CHECK_inputs(inputs, "VERTEX");
	domUint maxoffset = GetMaxOffsetFromInputs(inputs);
	domPRef p = triangles->getP();
	domListOfUInts & ints = p->getValue();

	// check count
	errorcount += CHECK_count(triangles, 3 * count * maxoffset, (domInt) ints.getCount(),
		                      "triangles, count doesn't match\n");

	// check index range
	for (domUint offset=0; offset<maxoffset; offset++)
	{
		domUint index_range = GetIndexRangeFromInput(inputs, offset);
		errorcount += CHECK_Index_Range (triangles, ints, index_range, offset, maxoffset);
	}
	return errorcount;
}
domUint CHECK_Polygons(domPolygons *polygons)
{
	domUint errorcount = 0;
	domUint count = polygons->getCount();
	domInputLocalOffset_Array & inputs = polygons->getInput_array();
	errorcount += CHECK_inputs(inputs, "VERTEX");
	domUint maxoffset = GetMaxOffsetFromInputs(inputs);
	domP_Array & parray = polygons->getP_array();

	// check count
	errorcount += CHECK_count(polygons, count, (domInt) parray.getCount(),
		                      "polygons, count doesn't match\n");

	// check index range
	for (size_t i=0; i<parray.getCount(); i++)
	{
		domListOfUInts & ints = parray[i]->getValue();
		for (domUint offset=0; offset<maxoffset; offset++)
		{
			domUint index_range = GetIndexRangeFromInput(inputs, offset);
			errorcount += CHECK_Index_Range (polygons, ints, index_range, offset, maxoffset);
		}
	}
	return errorcount;
}
domUint CHECK_Polylists(domPolylist *polylist)
{
	domUint errorcount = 0;
	domUint count = polylist->getCount();
	domInputLocalOffset_Array & inputs = polylist->getInput_array();
	errorcount += CHECK_inputs(inputs, "VERTEX");
	domUint maxoffset = GetMaxOffsetFromInputs(inputs);
	domPRef p = polylist->getP();
	domPolylist::domVcountRef vcount = polylist->getVcount();

	// check vcount
	errorcount += CHECK_count(polylist, count, (domInt) vcount->getValue().getCount(),
		                      "polylists, count doesn't match\n");
	// check p count
	domUint vcountsum = 0;
	for (size_t i=0; i<count; i++)
	{
		vcountsum += vcount->getValue()[i];
	}
	errorcount += CHECK_count(polylist, (domInt) p->getValue().getCount(), vcountsum * maxoffset,
		                      "polylists, total vcount and p count doesn't match\n");

	// check index range
	for (domUint offset=0; offset<maxoffset; offset++)
	{
		domUint index_range = GetIndexRangeFromInput(inputs, offset);
		errorcount += CHECK_Index_Range (polylist, p->getValue(), index_range, offset, maxoffset);
	}
	return errorcount;
}
domUint CHECK_Tristrips(domTristrips *tristrips)
{
	domUint errorcount = 0;
	domUint count = tristrips->getCount();
	domInputLocalOffset_Array & inputs = tristrips->getInput_array();
	errorcount += CHECK_inputs(inputs, "VERTEX");
	domUint maxoffset = GetMaxOffsetFromInputs(inputs);
	domP_Array & parray = tristrips->getP_array();

	// check vcount
	errorcount += CHECK_count(tristrips, count, (domInt) parray.getCount(),
		                      "tristrips, count doesn't match\n");
	// check p count
	for (size_t i=0; i<count; i++)
	{
		errorcount += CHECK_count(tristrips, 3 * maxoffset <= parray[i]->getValue().getCount(), 1,
			                      "tristrips, this p has less than 3 vertices\n");		
		errorcount += CHECK_count(tristrips, (domInt) parray[i]->getValue().getCount() % maxoffset, 0,
			                      "tristrips, this p count is not in multiple of maxoffset\n");		
	}

	// check index range
	for (size_t i=0; i<parray.getCount(); i++)
	{
		domListOfUInts & ints = parray[i]->getValue();
		for (domUint offset=0; offset<maxoffset; offset++)
		{
			domUint index_range = GetIndexRangeFromInput(inputs, offset);
			errorcount += CHECK_Index_Range (tristrips, ints, index_range, offset, maxoffset);
		}
	}
	return errorcount;
}
domUint CHECK_Trifans(domTrifans *trifans)
{
	domUint errorcount = 0;
	domUint count = trifans->getCount();
	domInputLocalOffset_Array & inputs = trifans->getInput_array();
	errorcount += CHECK_inputs(inputs, "VERTEX");
	domUint maxoffset = GetMaxOffsetFromInputs(inputs);
	domP_Array & parray = trifans->getP_array();

	// check vcount
	errorcount += CHECK_count(trifans, count, (domInt) parray.getCount(),
		                      "trifan, count doesn't match\n");
	// check p count
	for (size_t i=0; i<count; i++)
	{
		errorcount += CHECK_count(trifans, 3 * maxoffset <= parray[i]->getValue().getCount(), 1,
			                      "trifan, this p has less than 3 vertices\n");		
		errorcount += CHECK_count(trifans, (domInt) parray[i]->getValue().getCount() % maxoffset, 0,
			                      "trifan, this p count is not in multiple of maxoffset\n");		
	}

	// check index range
	for (size_t i=0; i<parray.getCount(); i++)
	{
		domListOfUInts & ints = parray[i]->getValue();
		for (domUint offset=0; offset<maxoffset; offset++)
		{
			domUint index_range = GetIndexRangeFromInput(inputs, offset);
			errorcount += CHECK_Index_Range (trifans, ints, index_range, offset, maxoffset);
		}
	}
	return errorcount;
}
domUint CHECK_Lines(domLines *lines)
{
	domUint errorcount = 0;
	domUint count = lines->getCount();
	domInputLocalOffset_Array & inputs = lines->getInput_array();
	errorcount = CHECK_inputs(inputs, "VERTEX");
	domUint maxoffset = GetMaxOffsetFromInputs(inputs);
	domP * p = lines->getP();
	// check p count
	errorcount += CHECK_count(lines, 2 * count * maxoffset, (domInt) p->getValue().getCount(),
		                      "lines, count doesn't match\n");

	// check index range
	for (domUint offset=0; offset<maxoffset; offset++)
	{
		domUint index_range = GetIndexRangeFromInput(inputs, offset);
		errorcount += CHECK_Index_Range (lines, p->getValue(), index_range, offset, maxoffset);
	}
	return errorcount;
}

domUint CHECK_Linestrips(domLinestrips *linestrips)
{
	domUint errorcount = 0;
	domUint count = linestrips->getCount();
	domInputLocalOffset_Array & inputs = linestrips->getInput_array();
	errorcount += CHECK_inputs(inputs, "VERTEX");
	domUint maxoffset = GetMaxOffsetFromInputs(inputs);
	domP_Array & parray = linestrips->getP_array();

	// check p count
	errorcount += CHECK_count(linestrips, count, (domInt) parray.getCount(),
		                      "linestrips, count doesn't match\n");
	// check inputs
	for (size_t i=0; i<count; i++)
	{
		errorcount += CHECK_count(linestrips, 2 * maxoffset <= parray[i]->getValue().getCount(), 1,
			                  "linestrips, this p has less than 2 vertices\n");		
		errorcount += CHECK_count(linestrips, (domInt) parray[i]->getValue().getCount() % maxoffset, 0,
			                  "linestrips, this p is not in mutiple of maxoffset\n");		
	}

	// check index range
	for (size_t i=0; i<parray.getCount(); i++)
	{
		domListOfUInts & ints = parray[i]->getValue();
		for (domUint offset=0; offset<maxoffset; offset++)
		{
			domUint index_range = GetIndexRangeFromInput(inputs, offset);
			errorcount += CHECK_Index_Range (linestrips, ints, index_range, offset, maxoffset);
		}
	}
	return errorcount;
}

domUint CHECK_Geometry(domGeometry *geometry)
{
	domUint errorcount = 0;
	domMesh * mesh = geometry->getMesh();
	if (mesh == NULL)
		return 0;

	// check vertices
	domVertices *vertices = mesh->getVertices();
	CHECK_error(geometry, vertices != NULL, "geometry, no vertices in this mesh\n");
	if (vertices)
	{
		domInputLocal_Array & inputs = vertices->getInput_array();
		errorcount += CHECK_inputs(inputs, "POSITION");
	}
	// triangles
	domTriangles_Array & triangles = mesh->getTriangles_array();
	for (size_t i=0; i<triangles.getCount(); i++)
	{
		errorcount += CHECK_Triangles(triangles[i]);
	}
	// polygons
	domPolygons_Array & polygons = mesh->getPolygons_array();
	for (size_t i=0; i<polygons.getCount(); i++)
	{
		errorcount += CHECK_Polygons(polygons[i]);
	}
	// polylist
	domPolylist_Array & polylists = mesh->getPolylist_array();
	for (size_t i=0; i<polylists.getCount(); i++)
	{
		errorcount += CHECK_Polylists(polylists[i]);
	}
	// tristrips
	domTristrips_Array & tristrips = mesh->getTristrips_array();
	for (size_t i=0; i<tristrips.getCount(); i++)
	{
		errorcount += CHECK_Tristrips(tristrips[i]);
	}
	// trifans
	domTrifans_Array & trifans = mesh->getTrifans_array();
	for (size_t i=0; i<trifans.getCount(); i++)
	{
		errorcount += CHECK_Trifans(trifans[i]);
	}
	// lines
	domLines_Array & lines = mesh->getLines_array();
	for (size_t i=0; i<lines.getCount(); i++)
	{
		errorcount += CHECK_Lines(lines[i]);
	}
	// linestrips
	domLinestrips_Array & linestrips = mesh->getLinestrips_array();
	for (size_t i=0; i<linestrips.getCount(); i++)
	{
		errorcount += CHECK_Linestrips(linestrips[i]);
	}
	return errorcount;
}

domUint CHECK_InstanceGeometry(domInstance_geometry * instance_geometry)
{
	domUint errorcount = 0;
	// check material binding to geometry symbols
	std::set<string> material_symbols;
	domBind_material * bind_material = instance_geometry->getBind_material();
	if (bind_material == NULL) {
		PRINTF("ERROR: CHECK_InstanceGeometry failed ");
		print_name_id(instance_geometry);
		PRINTF("\n  no bind materials in instance geometry \n");
		return 0;
	}
	if (bind_material->getTechnique_common() == NULL) {
		PRINTF("ERROR: CHECK_InstanceGeometry failed ");
		print_name_id(instance_geometry);
		PRINTF("\n  no technique_common in bind materials \n");
		return 0;
	}
	domInstance_material_Array & imarray = bind_material->getTechnique_common()->getInstance_material_array();
	for (size_t i=0; i<imarray.getCount(); i++)
	{
		material_symbols.insert(string(imarray[i]->getSymbol()));		
	}
	xsAnyURI & uri = instance_geometry->getUrl();
	domGeometry * geometry = (domGeometry *) (domElement*) uri.getElement();
	if (geometry == NULL) return errorcount ;
	domMesh * mesh = geometry->getMesh();
	if (mesh) {
		domTriangles_Array & triangles = mesh->getTriangles_array();
		for (size_t i=0; i<triangles.getCount(); i++)
		{
			daeString material_group = triangles[i]->getMaterial();
			if (material_group)
				CHECK_warning(instance_geometry, 
						material_symbols.find(material_group) != material_symbols.end(),
						"binding not found for material symbol");
		}
		domPolygons_Array & polygons = mesh->getPolygons_array();
		for (size_t i=0; i<polygons.getCount(); i++)
		{
			daeString material_group = polygons[i]->getMaterial();
			if (material_group)
				CHECK_warning(instance_geometry, 
						material_symbols.find(material_group) != material_symbols.end(),
						"binding not found for material symbol");
		}
		domPolylist_Array & polylists = mesh->getPolylist_array();
		for (size_t i=0; i<polylists.getCount(); i++)
		{
			daeString material_group = polylists[i]->getMaterial();
			if (material_group)
				CHECK_warning(instance_geometry, 
						material_symbols.find(material_group) != material_symbols.end(),
						"binding not found for material symbol");
		}
		domTristrips_Array & tristrips = mesh->getTristrips_array();
		for (size_t i=0; i<tristrips.getCount(); i++)
		{
			daeString material_group = tristrips[i]->getMaterial();
			if (material_group)
				CHECK_warning(instance_geometry, 
						material_symbols.find(material_group) != material_symbols.end(),
						"binding not found for material symbol");
		}
		domTrifans_Array & trifans = mesh->getTrifans_array();
		for (size_t i=0; i<trifans.getCount(); i++)
		{
			daeString material_group = trifans[i]->getMaterial();
			if (material_group)
				CHECK_warning(instance_geometry, 
						material_symbols.find(material_group) != material_symbols.end(),
						"binding not found for material symbol");
		}
		domLines_Array & lines = mesh->getLines_array();
		for (size_t i=0; i<lines.getCount(); i++)
		{
			daeString material_group = lines[i]->getMaterial();
			if (material_group)
				CHECK_warning(instance_geometry, 
						material_symbols.find(material_group) != material_symbols.end(),
						"binding not found for material symbol");
		}
		domLinestrips_Array & linestrips = mesh->getLinestrips_array();
		for (size_t i=0; i<linestrips.getCount(); i++)
		{
			daeString material_group = linestrips[i]->getMaterial();
			if (material_group)
				CHECK_warning(instance_geometry, 
						material_symbols.find(material_group) != material_symbols.end(),
						"binding not found for material symbol");
		}
	}
	return 0;
}

domUint GetVertexCountFromGeometry(domGeometry * geometry)
{
	domMesh * mesh = geometry->getMesh();
	if (mesh)
	{
		domVertices * vertices = mesh->getVertices();
		if (vertices)
		{
			domInputLocal_Array & inputs = vertices->getInput_array();
			for (size_t i=0; i<inputs.getCount(); i++)
			{
				if (stricmp(inputs[i]->getSemantic(), "POSITION") == 0)
				{
					domSource * source = (domSource*) (domElement*) inputs[i]->getSource().getElement();
					if(source)
					{
						domFloat_array * float_array = source->getFloat_array();
						if (float_array)
							return float_array->getCount();
					}
				}
			}
		}
	}
	PRINTF("ERROR: Can't get Vertices Count from geometry, something wrong here\n");
	return 0;
}

domUint CHECK_Controller(domController *controller)
{
	domUint errorcount = 0;
	domSkin * skin = controller->getSkin();
	if (skin)
	{
		xsAnyURI & uri = skin->getSource();

		domElement * element = uri.getElement();
		if (element == 0)
		{
			errorcount += CHECK_error(skin, element != 0, "can't resolve skin source\n");
			return errorcount;
		}
		daeString type_str = element->getTypeName();

		if (stricmp(type_str, "geometry") == 0)
		{	// skin is reference directly to geometry
			// get vertex count from skin
			domSkin::domVertex_weights * vertex_weights = skin->getVertex_weights();
			domUint vertex_weights_count = vertex_weights->getCount();
			domGeometry * geometry = (domGeometry*) (domElement*) uri.getElement();
			domMesh * mesh = geometry->getMesh();				
			if (mesh)
			{	// get vertex count from geometry
				domVertices * vertices = mesh->getVertices();
				CHECK_error(geometry, vertices != NULL, "geometry, vertices in this mesh\n");
				if (vertices)
				{
					xsAnyURI src = vertices->getInput_array()[0]->getSource();
					domSource * source = (domSource*) (domElement*) src.getElement();
					domUint vertices_count = source->getTechnique_common()->getAccessor()->getCount();
					errorcount += CHECK_count(controller, vertices_count, vertex_weights_count,
											"controller, vertex weight count != mesh vertex count\n");
				}
			}	// TODO: it could be convex_mesh and spline
			domUint vcount_count = (domUint) vertex_weights->getVcount()->getValue().getCount();
			errorcount += CHECK_count(controller, vcount_count, vertex_weights_count,
									  "controller, vcount count != vertex weight count\n");	
			domInputLocalOffset_Array & inputs = vertex_weights->getInput_array();
			domUint sum = 0;
			for (size_t i=0; i<vcount_count; i++)
			{
				sum += vertex_weights->getVcount()->getValue()[i];
			}
			errorcount += CHECK_count(controller, sum * inputs.getCount(), (domInt) vertex_weights->getV()->getValue().getCount(), 
									  "controller, total vcount doesn't match with numbers of v\n");

			// check index range on <v>
			domListOfInts & ints = vertex_weights->getV()->getValue();
			domUint maxoffset = GetMaxOffsetFromInputs(inputs);
			for (size_t j=0; j<maxoffset; j++)
			{
				domUint index_range = GetIndexRangeFromInput(inputs, j);
				CHECK_Index_Range(skin, ints, index_range, j, maxoffset);
			}
		}
	}
	domMorph * morph = controller->getMorph();
	if (morph)
	{
		domUint source_geometry_vertices_count = 0;
		xsAnyURI & uri = morph->getSource();
		domElement * element = uri.getElement();
		if (element == 0)
		{
			errorcount++;
			PRINTF("ERROR: MORPH Source base mesh element does not resolve\n");
			return errorcount;
		}
		daeString type_str = element->getTypeName();
		if (stricmp(type_str, "geometry") == 0)
		{
			domGeometry * source_geometry = (domGeometry *) element;
			source_geometry_vertices_count = GetVertexCountFromGeometry(source_geometry);
		}
		domInputLocal_Array & inputs = morph->getTargets()->getInput_array();
		for (size_t i=0; i<inputs.getCount(); i++)
		{
			if(stricmp(inputs[i]->getSemantic(), "MORPH_TARGET") == 0)
			{
				domSource * source = (domSource*) (domElement*) inputs[i]->getSource().getElement();
				domIDREF_array * IDREF_array = source->getIDREF_array();
				if(IDREF_array)
				{
					xsIDREFS & ifrefs = IDREF_array->getValue();
					for (size_t j=0; j<ifrefs.getCount(); j++)
					{
						domElement * element = ifrefs[j].getElement();
						domGeometry * target_geometry = (domGeometry*) element;
						domUint target_geo_vertices_count = GetVertexCountFromGeometry(target_geometry);
						if (source_geometry_vertices_count !=target_geo_vertices_count)
						{
							errorcount++;
							PRINTF("ERROR: MORPH Target vertices counts != MORPH Source vertices counts\n");
						}
					}
				}
			}
		}
	}
	return errorcount;
}

domUint CHECK_InstanceElementUrl(daeDatabase *db, const char * instance_element) {
	domUint errorcount = 0;
	for (daeInt i=0; i<(daeInt)db->getElementCount(NULL, instance_element, file_name.c_str() ); i++)
	{
		domInstanceWithExtra *element = 0;
		domInt error = db->getElement((daeElement**)&element, i, NULL, instance_element, file_name.c_str());
		if (error == DAE_OK) {
			errorcount += CHECK_uri(element->getUrl());
		}
	}
	return errorcount;
}

domUint GetSizeFromType(xsNMTOKEN type)
{
	if (stricmp(type, "bool2")==0)
		return 2;
	else if (stricmp(type, "bool3")==0)
		return 3;
	else if (stricmp(type, "bool4")==0)
		return 4;
	else if (stricmp(type, "int2")==0)
		return 2;
	else if (stricmp(type, "int3")==0)
		return 3;
	else if (stricmp(type, "int4")==0)
		return 4;
	else if (stricmp(type, "float2")==0)
		return 2;
	else if (stricmp(type, "float3")==0)
		return 3;
	else if (stricmp(type, "float4")==0)
		return 4;
	else if (stricmp(type, "float2x2")==0)
		return 4;
	else if (stricmp(type, "float3x3")==0)
		return 9;
	else if (stricmp(type, "float4x4")==0)
		return 16;
	return 1;
}

domUint CHECK_Source(domSource * source)
{
	domUint errorcount = 0;
	// check if this is a source with children
	daeTArray<daeSmartRef<daeElement> >  children;
	source->getChildren(children);
	if (children.getCount() <= 0) return 0;
	// prepare technique_common 
	domSource::domTechnique_common * technique_common = source->getTechnique_common();
	domAccessor * accessor = 0;
	domUint accessor_count = 0;
	domUint accessor_stride = 0;
	domUint accessor_size = 0;
	domUint array_count = 0;
	domUint array_value_count = 0;
	if (technique_common)
	{
		accessor = technique_common->getAccessor();
		if (accessor)
		{
			accessor_count = accessor->getCount();
			accessor_stride = accessor->getStride();
			domParam_Array & param_array = accessor->getParam_array();
			for(size_t i=0; i<param_array.getCount(); i++)
			{
				xsNMTOKEN type = param_array[i]->getType();
				accessor_size += GetSizeFromType(type);
			}
			errorcount += CHECK_count(source, accessor_size, accessor_stride, "accessor stride != total size of all param\n"); 
		}
	}

	// float_array
	domFloat_array * float_array = source->getFloat_array();
	if (float_array)
	{
		array_count = float_array->getCount(); 
		array_value_count = (domUint) float_array->getValue().getCount(); 
	}
	
	// int_array
	domInt_array * int_array = source->getInt_array();
	if (int_array)
	{
		array_count = int_array->getCount(); 
		array_value_count = (domUint) int_array->getValue().getCount(); 
	} 
	
	// bool_array
	domBool_array * bool_array = source->getBool_array();
	if (bool_array)
	{
		array_count = bool_array->getCount(); 
		array_value_count = (domUint) bool_array->getValue().getCount(); 
	}
	
	// idref_array
	domIDREF_array * idref_array = source->getIDREF_array();
	if (idref_array)
	{
		array_count = idref_array->getCount(); 
		array_value_count = (domUint) idref_array->getValue().getCount(); 
	}
	
	// name_array
	domName_array * name_array = source->getName_array();
	if (name_array)
	{
		array_count = name_array->getCount(); 
		array_value_count = (domUint) name_array->getValue().getCount(); 
	}  
	errorcount += CHECK_count(source, array_count, array_value_count,
			                    "array count != number of name in array value_count\n");
	if (accessor)
	{
		errorcount += CHECK_count(source, array_count, accessor_count * accessor_stride, 
								"accessor_stride >= accessor_size but array_count != accessor_count * accessor_stride\n");
	}
	
	return errorcount ;
}

domUint CHECK_counts (DAE *input, int verbose)
{
//	checklist = new std::map<daeString, domElement *>;
	domInt error = 0;
	domUint errorcount = 0;
	daeDatabase *db = input->getDatabase();

	// check geometry
	daeInt count = (daeInt) db->getElementCount(NULL, "geometry", file_name.c_str() );
	for (daeInt i=0; i<count; i++)
	{
		domGeometry *geometry;
		error = db->getElement((daeElement**)&geometry, i, NULL, "geometry", file_name.c_str());
		errorcount += CHECK_Geometry(geometry);			
	}
	// check controller
	count = (daeInt)db->getElementCount(NULL, "controller", file_name.c_str() );
	for (daeInt i=0; i<count; i++)
	{
		domController *controller;
		error = db->getElement((daeElement**)&controller, i, NULL, "controller", file_name.c_str());
		errorcount += CHECK_Controller(controller);			
	}
	// check instance_geometry
	count = (daeInt)db->getElementCount(NULL, "instance_geometry", file_name.c_str() );
	for (daeInt i=0; i<count; i++)
	{
		domInstance_geometry *instance_geometry;
		error = db->getElement((daeElement**)&instance_geometry, i, NULL, "instance_geometry", file_name.c_str() );
		errorcount += CHECK_InstanceGeometry(instance_geometry);
	}
	// check source
	count = (daeInt)db->getElementCount(NULL, "source", file_name.c_str() );
	for (daeInt i=0; i<count; i++)
	{
		domSource *source;
		error = db->getElement((daeElement**)&source, i, NULL, "source", file_name.c_str() );
		errorcount += CHECK_Source(source);
	}
	return errorcount; 
}

domUint CHECK_links (DAE *input, int verbose)
{
	domInt error = 0;
	domUint errorcount = 0;
	daeDatabase *db = input->getDatabase();

	// check links
	daeInt count = (daeInt)db->getElementCount(NULL, "accessor", file_name.c_str() );
	for (daeInt i=0; i<count; i++)
	{
		domAccessor *accessor;
		error = db->getElement((daeElement**)&accessor, i, NULL, "accessor", file_name.c_str() );
		xsAnyURI & uri = accessor->getSource();
		errorcount += CHECK_uri(uri);
	}
	count = (daeInt)db->getElementCount(NULL, "channel", file_name.c_str());
	for (daeInt i=0; i<count; i++)
	{
		domChannel *channel;
		error = db->getElement((daeElement**)&channel, i, NULL, "channel", file_name.c_str());
		xsAnyURI & uri = channel->getSource();
		errorcount += CHECK_uri(uri);
	}
	count = (daeInt)db->getElementCount(NULL, "IDREF_array", file_name.c_str());
	for (daeInt i=0; i<count; i++)
	{
		domIDREF_array *IDREF_array;
		error = db->getElement((daeElement**)&IDREF_array, i, NULL, "IDREF_array", file_name.c_str());
		for (size_t j=0; j<IDREF_array->getCount(); j++) 
		{
			daeIDRef idref = IDREF_array->getValue()[j];
			idref.resolveElement();
			domElement * element = idref.getElement();
			if (element == NULL)
			{
				char temp[MAX_LOG_BUFFER];
				sprintf(temp, "IDREF_array value %s not referenced\n", idref.getID());
				PRINTF(temp);
				errorcount += CHECK_error(IDREF_array, element!=NULL, temp);
			}
		}
	}
	count = (daeInt)db->getElementCount(NULL, "input", file_name.c_str());
	for (daeInt i=0; i<count; i++)
	{
		domInputLocalOffset *input;
		error = db->getElement((daeElement**)&input, i, NULL, "input", file_name.c_str());
		xsAnyURI & uri = input->getSource();
		errorcount += CHECK_uri(uri);
	}

	count = (daeInt)db->getElementCount(NULL, "skeleton", file_name.c_str());
	for (daeInt i=0; i<count; i++)
	{
		domInstance_controller::domSkeleton *skeleton;
		error = db->getElement((daeElement**)&skeleton, i, NULL, "skeleton", file_name.c_str());
		xsAnyURI & uri = skeleton->getValue();
		errorcount += CHECK_uri(uri);
	}
	count = (daeInt)db->getElementCount(NULL, "skin", file_name.c_str());
	for (daeInt i=0; i<count; i++)
	{
		domSkin *skin;
		error = db->getElement((daeElement**)&skin, i, NULL, "skin", file_name.c_str());
		xsAnyURI & uri = skin->getSource();
		errorcount += CHECK_uri(uri);
	}
	// physics
/*	for (size_t i=0; i<db->getElementCount(NULL, "program", NULL); i++)
	{
		domProgram *program;
		error = db->getElement((daeElement**)&program, i, NULL, "program");
		xsAnyURI & uri = program->getSource();
		errorcount += CHECK_uri(uri);
	}
*/
	count = (daeInt)db->getElementCount(NULL, "instance_rigid_body", file_name.c_str());
	for (daeInt i=0; i<count; i++)
	{
		domInstance_rigid_body *instance_rigid_body;
		error = db->getElement((daeElement**)&instance_rigid_body, i, NULL, "instance_rigid_body", file_name.c_str());
		xsAnyURI & uri = instance_rigid_body->getTarget();
		errorcount += CHECK_uri(uri);
	}
	count = (daeInt)db->getElementCount(NULL, "ref_attachment", file_name.c_str());
	for (daeInt i=0; i<count; i++)
	{
		domRigid_constraint::domRef_attachment *ref_attachment;
		error = db->getElement((daeElement**)&ref_attachment, i, NULL, "ref_attachment", file_name.c_str());
		xsAnyURI & uri = ref_attachment->getRigid_body();
		errorcount += CHECK_uri(uri);
	}

	// FX, todo: color_target, connect_param, depth_target, param, stencil_target
	count = (daeInt)db->getElementCount(NULL, "instance_material", file_name.c_str());
	for (daeInt i=0; i<count; i++)
	{
		domInstance_material *instance_material;
		error = db->getElement((daeElement**)&instance_material, i, NULL, "instance_material", file_name.c_str());
		xsAnyURI & uri = instance_material->getTarget();
		errorcount += CHECK_uri(uri);
	}


	// urls
	domUint instance_elments_max = 12;
	const char * instance_elments[] = { "instance_animation",
										"instance_camera", 
										"instance_controller", 
										"instance_geometry", 
										"instance_light", 
										"instance_node", 
										"instance_visual_scene", 
										"instance_effect",
										"instance_force_field", 
										"instance_physics_material",
										"instance_physics_model",
										"instance_physics_scene"};
	
	for (size_t i=0; i<instance_elments_max ; i++) 
	{
		errorcount += CHECK_InstanceElementUrl(db, instance_elments[i]);
	}
	return 0;
}


domUint CHECK_files (DAE *input, int verbose)
{
	domInt error = 0;
	domUint errorcount = 0;
	daeDatabase *db = input->getDatabase();

	// files
	daeInt count = (daeInt) db->getElementCount(NULL, "image", file_name.c_str());
	for (daeInt i=0; i<count; i++)
	{
		domImage *image;
		error = db->getElement((daeElement**)&image, i, NULL, "image", file_name.c_str());
		domImage::domInit_from * init_from = image->getInit_from();
		xsAnyURI & uri = init_from->getValue();
		errorcount += CHECK_file(init_from, uri);
	}
	count = (daeInt) db->getElementCount(NULL, "include", file_name.c_str());
	for (daeInt i=0; i<count; i++)
	{
		domFx_include_common *include;
		error = db->getElement((daeElement**)&include, i, NULL, "include", file_name.c_str());
		xsAnyURI & uri = include->getUrl();
		errorcount += CHECK_file(include, uri);
	}
	return errorcount;
}

domUint CHECK_escape_char(daeString str)
{
	domUint errorcount = 0;
	size_t len = strlen(str);
	for(size_t i=0; i<len; i++)
	{
		switch(str[i])
		{
		case ' ':
		case '#':
		case '$':
		case '%':
		case '&':
		case '/':
		case ':':
		case ';':
		case '<':
		case '=':
		case '>':
		case '?':
		case '@':
		case '[':
		case '\\':
		case ']':
		case '^':
		case '`':
		case '{':
		case '|':
		case '}':
		case '~':
			char temp[1024];
			sprintf(temp, "ERROR: string '%s' contains non-escaped char '%c'\n", str, str[i]);
			PRINTF(temp);
			errorcount++;
		default:
			continue;
		}
	}
	return errorcount;
}

domUint CHECK_unique_id (DAE *input, int verbose)
{
	std::pair<std::set<string>::iterator, bool> pair;
	std::set<string> ids;
	domInt error = 0;
	domUint errorcount = 0;
	daeDatabase *db = input->getDatabase();
	daeInt count = (daeInt) db->getElementCount(NULL, NULL, NULL);
	for (daeInt i=0; i<count; i++)
	{
		domElement *element;
		error = db->getElement((daeElement**)&element, i, NULL, NULL, NULL);
		daeString id = element->getID();
		if (id == NULL) continue;

		errorcount += CHECK_escape_char(id);

		daeString docURI = element->getDocumentURI()->getURI();
		// check if there is second element with the same id.
		error = db->getElement((daeElement**)&element, 1, id, NULL, docURI);
		if (error == DAE_OK)
		{
			errorcount++;
			char temp[MAX_LOG_BUFFER];
			sprintf(temp, "ERROR: Unique ID conflict id=%s, docURI=%s\n", id, docURI);
			PRINTF(temp);
		}
	}
	return errorcount;
}

domEffect *TextureGetEffect(domCommon_color_or_texture_type::domTexture *texture)
{
	for (domElement * element = texture; element; element = element->getParentElement())
	{
		if (element->getTypeName())
			if (stricmp(element->getTypeName(), "effect") == 0)
				return (domEffect *)element;
	}
	return NULL;
}

domUint CHECK_texture (DAE *input, int verbose)
{
	std::pair<std::set<string>::iterator, bool> pair;
	std::set<string> ids;
	domInt error = 0;
	domUint errorcount = 0;
	daeDatabase *db = input->getDatabase();

	daeInt count = (daeInt) db->getElementCount(NULL, "texture", file_name.c_str());
	for (daeInt i=0; i<count; i++)
	{
		domCommon_color_or_texture_type::domTexture *texture;
		error = db->getElement((daeElement**)&texture, i, NULL, "texture", file_name.c_str());
		xsNCName texture_name = texture->getTexture();
		char * target = new char[ strlen( texture_name ) + 3 ];
		strcpy( target, "./" );
		strcat( target, texture_name );
		domEffect * effect = TextureGetEffect(texture);
		if (effect==NULL) 
			continue;
		daeSIDResolver sidRes( effect, target );
		delete[] target;
		target = NULL;
		if ( sidRes.getElement() != NULL )
		{	// it is doing the right way
			continue;
		}
		daeIDRef ref(texture_name);
		ref.setContainer(texture);
		ref.resolveElement();
		daeElement * element = ref.getElement();
		if (element)
		{	// it is directly linking the image
			char temp[MAX_LOG_BUFFER];
			sprintf(temp, "ERROR: CHECK_texture failed, texture=%s is direct linking to image\n", texture_name);
			PRINTF(temp);
		} else {
			char temp[MAX_LOG_BUFFER];
			sprintf(temp, "ERROR: CHECK_texture failed, texture=%s is not link to anything\n", texture_name);
			PRINTF(temp);
		}
	}
	return errorcount;
}

void _XMLSchemaValidityErrorFunc(void* ctx, const char* msg, ...)
{
	va_list      LVArgs;
	char         LTmpStr[MAX_LOG_BUFFER];    // FIXME: this is not buffer-overflow safe
	memset(LTmpStr,0,MAX_LOG_BUFFER);

//	DAEDocument* LDAEDocument = (DAEDocument*)ctx;
//	xmlSchemaValidCtxt* LXMLSchemaValidContext = (xmlSchemaValidCtxt*) ctx;
//	xmlNode*     LXMLNode = xmlSchemaValidCtxtGetNode(ctx);
//	xmlDoc *	 doc = (xmlDoc *) ctx;

	va_start(LVArgs, msg);
	vsprintf(LTmpStr, msg, LVArgs);
	va_end(LVArgs);
//	PRINTF("%s:%d  Schema validation error:\n%s", LDAEDocument->Name, xmlGetLineNo(LXMLNode), LTmpStr);
//	PRINTF("CHECK_schema Error msg=%c ctx=%p\n", msg, ctx);
	char temp[MAX_LOG_BUFFER];
	memset(temp,0,MAX_LOG_BUFFER);
	sprintf(temp, "ERROR: CHECK_schema Error   msg=%s\n", LTmpStr);
 	PRINTF(temp);
}


void _XMLSchemaValidityWarningFunc(void* ctx, const char* msg, ...)
{
	va_list      LVArgs;
	char         LTmpStr[MAX_LOG_BUFFER];    // FIXME: this is not buffer-overflow safe
	memset(LTmpStr,0,MAX_LOG_BUFFER);
//	DAEDocument* LDAEDocument = (DAEDocument*)ctx;
//	xmlNode*     LXMLNode = xmlSchemaValidCtxtGetNode(LDAEDocument->XMLSchemaValidContext);
//	xmlDoc *	 doc = (xmlDoc *) ctx;

	va_start(LVArgs, msg);
	vsprintf(LTmpStr, msg, LVArgs);
	va_end(LVArgs);
//	PRINTF("%s:%d  Schema validation warning:\n%s", LDAEDocument->Name, xmlGetLineNo(LXMLNode), LTmpStr);
	char temp[MAX_LOG_BUFFER];
	memset(temp,0,MAX_LOG_BUFFER);
	sprintf(temp, "ERROR: CHECK_schema Warning msg=%s\n", LTmpStr);
	PRINTF(temp);
}

//void dae_ValidateDocument(DAEDocument* LDAEDocument, xmlDocPtr LXMLDoc)
domUint CHECK_validateDocument(xmlDocPtr LXMLDoc)
{
//	const char * dae_SchemaURL = "C:\\svn\\COLLADA_DOM\\doc\\COLLADASchema.xsd";
	const char * dae_SchemaURL = "http://www.collada.org/2005/11/COLLADASchema.xsd";

	xmlSchemaParserCtxt*  Ctxt = xmlSchemaNewDocParserCtxt(LXMLDoc);
	xmlSchemaParserCtxt*    LXMLSchemaParserCtxt = xmlSchemaNewParserCtxt(dae_SchemaURL);

	if(LXMLSchemaParserCtxt)
	{
		xmlSchema*    LXMLSchema = xmlSchemaParse(LXMLSchemaParserCtxt);

		if(LXMLSchema)
		{
			xmlSchemaValidCtxt*    LXMLSchemaValidContext = xmlSchemaNewValidCtxt(LXMLSchema);

			if(LXMLSchemaValidContext)
			{	
				int    LSchemaResult;

//				LDAEDocument->XMLSchemaValidContext = LXMLSchemaValidContext;

/*				xmlSchemaSetParserErrors(LXMLSchemaParserCtxt,
							_XMLSchemaValidityErrorFunc,
							_XMLSchemaValidityWarningFunc,
							LXMLDoc);
*/
//				globalpointer = this;
				xmlSchemaSetValidErrors(LXMLSchemaValidContext,
							_XMLSchemaValidityErrorFunc,
							_XMLSchemaValidityWarningFunc,
							LXMLDoc);

				LSchemaResult = xmlSchemaValidateDoc(LXMLSchemaValidContext, LXMLDoc);
//				switch(LSchemaResult)
//				{
//				case 0:    PRINTF("Document validated\n");break;
//				case -1:   PRINTF("API error\n");break;
//				default:   PRINTF("Error code: %d\n", LSchemaResult);break;
//				}

//			LDAEDocument->XMLSchemaValidContext = NULL;
			xmlSchemaFreeValidCtxt(LXMLSchemaValidContext);
			}

			xmlSchemaFree(LXMLSchema);
		}
		else {
			char temp[MAX_LOG_BUFFER];
			sprintf(temp, "ERROR: Could not parse schema from %s\n", dae_SchemaURL);
			PRINTF(temp);
		}
		xmlSchemaFreeParserCtxt(LXMLSchemaParserCtxt);
	}
	else {
		char temp[MAX_LOG_BUFFER];
		sprintf(temp, "ERROR: Could not load schema from '%s\n", dae_SchemaURL);
		PRINTF(temp);
	}
	return 0;
}


domUint CHECK_xmlfile(daeString filename)
{
	xmlDocPtr doc;		
	doc = xmlReadFile(filename, NULL, 0);
    if (doc == NULL) {
		char temp[MAX_LOG_BUFFER];
		sprintf(temp, "ERROR: Failed to parse %s\n", filename);
		PRINTF(temp);
		return 1;
    } else {
		// load okay
		CHECK_validateDocument(doc);
		/* free up the parser context */
		xmlFreeDoc(doc);
	}
	return 0;
}

domUint CHECK_schema (DAE *input, int verbose)
{
	domInt errorcount = 0;
	daeDatabase* db = input->getDatabase();
	daeInt count = (daeInt) db->getDocumentCount();
	for (daeInt i=0; i<count; i++)
	{
		daeDocument* doc = db->getDocument(i);
		daeString xmldoc = doc->getDocumentURI()->getURI();
		errorcount += CHECK_xmlfile(xmldoc);
	}
	return errorcount;
}

domUint CHECK_inputs (domInputLocalOffset_Array & inputs, const char * semantic)
{
	domUint count = (domUint) inputs.getCount();
	for (size_t i=0; i<count; i++) 
	{
		if (stricmp(semantic, inputs[i]->getSemantic()) == 0)
			return 0;
	}
	PRINTF("ERROR: CHECK inputs Failed ");
	print_name_id(inputs[0]);
	char temp[MAX_LOG_BUFFER];
	sprintf(temp, " input with semantic=%s not found\n", semantic);
	PRINTF(temp);
	return 1;
}

domUint CHECK_inputs (domInputLocal_Array & inputs, const char * semantic)
{
	domUint count = (domUint) inputs.getCount();
	for (size_t i=0; i<count; i++) 
	{
		if (stricmp(semantic, inputs[i]->getSemantic()) == 0)
			return 0;
	}
	PRINTF("ERROR: CHECK inputs Failed ");
	print_name_id(inputs[0]);
	char temp[MAX_LOG_BUFFER];
	sprintf(temp, " input with semantic=%s not found\n", semantic);
	PRINTF(temp);
	return 1;
}

domUint CHECK_skin (DAE *input, int verbose)
{
	std::vector<domNode *> nodeset;
	domInt error = 0;
	domUint errorcount = 0;
	daeDatabase *db = input->getDatabase();

	daeInt count = (daeInt) db->getElementCount(NULL, "instance_controller", file_name.c_str());
	for (daeInt i=0; i<count; i++)
	{
		domInstance_controller *instance_controller;
		error = db->getElement((daeElement**)&instance_controller, i, NULL, "instance_controller", file_name.c_str());

		// get all skeletons
		domInstance_controller::domSkeleton_Array & skeleton_array = instance_controller->getSkeleton_array();
		domUint skeleton_count = (domUint) skeleton_array.getCount();
		for (size_t i=0; i<skeleton_count; i++)
		{
			domNode * node = (domNode*) (domElement*) skeleton_array[i]->getValue().getElement();
			nodeset.push_back(node);
		}

		//get joints from skin
		domController * controller = (domController*) (domElement*) instance_controller->getUrl().getElement();
		domSkin * skin = controller->getSkin();
		if (skin)
		{
			// get source of name_array or IDREF_array
			domSource * joint_source = NULL;
			domSource * inv_bind_matrix_source = NULL;
			domSkin::domJoints * joints = skin->getJoints();
			domInputLocal_Array &input_array = joints->getInput_array();
			for(size_t i=0; i<input_array.getCount(); i++)
			{
				if (stricmp(input_array[i]->getSemantic(), "JOINT") == 0)
				{
					joint_source = (domSource *) (domElement*) input_array[i]->getSource().getElement();
					if (joint_source) continue;
				} else if (stricmp(input_array[i]->getSemantic(), "INV_BIND_MATRIX") == 0)
				{
					inv_bind_matrix_source = (domSource *) (domElement*) input_array[i]->getSource().getElement();
					if (inv_bind_matrix_source) continue;					
				}
			}
			if (joint_source == NULL) continue;
			if (inv_bind_matrix_source == NULL) continue;

			//check count of joint source and inv_bind_matrix_source
			domSource::domTechnique_common * techique_common = NULL;
			domAccessor * accessor = NULL;
			domUint joint_count = 0;
			domUint inv_bind_matrix_count = 0;
			techique_common = joint_source->getTechnique_common();
			if (techique_common) 
			{
				accessor = techique_common->getAccessor();
				if (accessor)
					domUint joint_count = accessor->getCount();
			}
			techique_common = inv_bind_matrix_source->getTechnique_common();
			if (techique_common) 
			{
				accessor = techique_common->getAccessor();
				if (accessor)
					domUint inv_bind_matrix_count = accessor->getCount();
			}
			errorcount += CHECK_count(skin, joint_count, inv_bind_matrix_count,
				"WARNING, joint count and inv bind matrix count does not match.\n");

			//name_array
			domName_array * name_array = joint_source ->getName_array();
			domIDREF_array * idref_array = joint_source ->getIDREF_array();
			if (name_array)
			{
				domListOfNames &list_of_names = name_array->getValue();
				domUint name_count = (domUint) list_of_names.getCount();
				for (domUint j=0; j<name_count; j++)
				{
					char jointpath[MAX_PATH];
					strcpy( jointpath, "./" );
					strcat( jointpath, list_of_names[(size_t)j] );
					domElement * e = NULL;
					for (size_t k=0; k<nodeset.size(); k++)
					{
						daeSIDResolver sidRes( nodeset[k], jointpath );
						e = sidRes.getElement();
						if (e) break;
					}
					if (e==NULL) // this joint name is not match with any sid of skeleton nodes
					{
						char tempstr[MAX_PATH];
						sprintf(tempstr, "instance_controller, can't find node with sid=%s of controller=%s\n", list_of_names[(size_t)j], controller->getId());
						errorcount += CHECK_error(instance_controller, false, tempstr);								
					}
				}
			} else if (idref_array)
			{
				xsIDREFS & list_of_idref = idref_array->getValue();
				domUint idref_count = (domUint) list_of_idref.getCount();
				for (domUint j=0; j<idref_count; j++)
				{
					list_of_idref[(size_t)j].resolveElement();
					domElement * element = list_of_idref[(size_t)j].getElement();
					if (element == 0)
					{
						char tempstr[MAX_PATH];
						sprintf(tempstr, "skin, idref=%s can't resolve\n", list_of_idref[(size_t)j].getID());
						errorcount +=CHECK_error(joint_source, false, tempstr);
					}
				}
			} else { // name_array and IDREF_array not found
				errorcount +=CHECK_error(skin, false, "skin, both name_array and IDREF_array are not found");
			}			
		} else // TODO: for other non-skin controllers
		{
		}
	}

	return errorcount;
}

domUint CHECK_float_array (DAE *input, int verbose)
{
	domInt count = 0;
	domInt error = 0;
	domInt errorcount=0;
	daeDatabase *db = input->getDatabase();
	count = (daeInt) db->getElementCount(NULL, COLLADA_ELEMENT_FLOAT_ARRAY, file_name.c_str());
	for (int i=0; i<count; i++)
	{
		domFloat_array * float_array;
		error = db->getElement((daeElement**)&float_array, i, NULL, COLLADA_ELEMENT_FLOAT_ARRAY, file_name.c_str());
		domListOfFloats & listoffloats = float_array->getValue();
		for (size_t j=0; j<listoffloats.getCount(); j++)
		{
			if ( (listoffloats[j] == 0x7f800002) ||
				 (listoffloats[j] == 0x7f800000) ||
				 (listoffloats[j] == 0xff800000) )
			{
				errorcount++;
				PRINTF("ERROR: Float_array contain either -INF, INF or NaN\n");
			}	 
		}
	}
	return errorcount;
}

enum eVISIT
{
    NOT_VISITED = 0,
    VISITING,
    VISITED,
};

domUint CHECK_node (domNode * parent_node, std::map<domNode*,eVISIT> * node_map)
{
//	eVISIT result = (*node_map)[parent_node];
	switch((*node_map)[parent_node])
	{
	case VISITING:// means we are visiting this node again "circular reference detected.
		PRINTF("WARNING: circular reference detected.\n");
		break;
	case VISITED: // means we already visited this node.
		return 0; 
	default:      // means we are visiting this node the first time.
		(*node_map)[parent_node] = VISITING;
		break;
	}

	domNode_Array & node_array = parent_node->getNode_array();
	for (size_t i=0; i<node_array.getCount(); i++)
	{
		domNode * node = node_array[i];
		if (node) CHECK_node(node, node_map);
	}
	
	domInstance_node_Array & instance_node_array = parent_node->getInstance_node_array();
	for (size_t i=0; i<instance_node_array.getCount(); i++)
	{
		domNode * node = (domNode*) (domElement*) instance_node_array[i]->getUrl().getElement();
		if (node) CHECK_node(node, node_map);
	}

/*	domInstance_controller_Array instance_controller_array & parent_node->getInstance_controller_array();
	for (size_t i=0; i<instance_controller_array.getCount(); i++)
	{
		instance_controller_array[i]->getSkeleton_array()
		domNode * node = (domNode*) (domElement*) ->getUrl().getElement();
		CHECK_node(node);
	}
*/
	
	(*node_map)[parent_node] = VISITED;
	return 0;
}

domUint CHECK_Circular_Reference (DAE *input, int verbose)
{
	domInt errorcount=0;

	std::map<domNode*,eVISIT> node_map;
	domCOLLADA * collada = input->getDom(file_name.c_str());
	domCOLLADA::domScene * scene = collada->getScene();
	if (scene == NULL) return 0;
	domInstanceWithExtra * instance_scene = scene->getInstance_visual_scene();
	if (instance_scene == NULL) return 0;
	domVisual_scene * visual_scene = (domVisual_scene*) (domElement*) instance_scene->getUrl().getElement();
	if (visual_scene == NULL) return 0;
	domNode_Array & node_array = visual_scene->getNode_array();
	for (size_t i=0; i<node_array.getCount(); i++)
	{
		domNode * node = node_array[i];
		CHECK_node(node, &node_map);
	}

	return errorcount;
}

domUint CHECK_Index_Range (domElement * elem, domListOfUInts & listofint, domUint index_range, domUint offset, domUint maxoffset, int verbose)
{
	if (checklist["INDEX_RANGE"] == false) return 0;
	domInt errorcount=0;
	for (size_t i=(size_t)offset; i<listofint.getCount(); i=(size_t)i+(size_t)maxoffset)
	{
		errorcount += CHECK_error(elem, listofint[i] < index_range, "ERROR: index out of range\n");			
	}
	return errorcount;
}
domUint CHECK_Index_Range (domElement * elem, domListOfInts & listofint, domUint index_range, domUint offset, domUint maxoffset, int verbose)
{
	if (checklist["INDEX_RANGE"] == false) return 0;
	domInt errorcount=0;
	for (size_t i=(size_t)offset; i<listofint.getCount(); i=(size_t)i+(size_t)maxoffset)
	{
		errorcount += CHECK_error(elem, listofint[i] < (domInt) index_range, "ERROR: index out of range\n");			
	}
	return errorcount;
}