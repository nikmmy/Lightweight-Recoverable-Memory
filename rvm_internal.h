
#include <iostream>

using namespace std;

/**Standard data structures**/
typedef const char* rvm_t;
typedef int trans_t;
typedef struct seg_entry{
	int size;
	char* address;
	int in_use;
	int is_mapped;
}seg_entry;

typedef struct redo_record
{
	const char* segname;
	int offset;
	char* new_data;

	/*redo_record(const char *Name, int off, char *d) : segname(Name), offset(off), new_data(d)
	    {}*/
}redo_record;

typedef struct undo_record
{
	const char* segname;
	int offset;
	int size;
	char* data;

	/*undo_record(const char *Name, int off, int s,char *d) : segname(Name), offset(off), size(s),data(d)
		    {}*/
}undo_record;
