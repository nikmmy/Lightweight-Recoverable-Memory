#include "rvm.h"
#include <stdlib.h>  
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <string>
#include <fstream>
#include <iostream>
#include <map>
#include <algorithm>
#include <vector>

using namespace std;

map<const char*, struct seg_entry> segments;
map<void*, const char*> segaddresstoname;
trans_t trans_count=0;

map<trans_t, vector<struct redo_record *> > redo_log;

map<trans_t, vector<struct undo_record *> > undo_log;

map<trans_t, rvm_t> trans_rvm;

int fileSize(rvm_t directory, const char *fileName)
{
	string dir(directory);
	string fil(fileName);
	string separator = "/";
	string full_path = dir + separator + fil;
	//cout << full_path;

	ifstream infile((const char*) full_path.c_str(), ios::binary | ios::ate);
	if (infile.good())
	{
		//cout << "File exists at" << full_path << endl;
		return infile.tellg();
	}
	else
	{
		//cout << "File does not exist at" << full_path << endl;
		return -1;
	}
}

string get_full_file_path(rvm_t rvm, const char*segname)
{
	string dir(rvm);
	string fil(segname);
	string separator = "/";
	string full_path = dir + separator + fil;
	return full_path;
}

rvm_t rvm_init(const char *directory)
{
	char* command = (char*) malloc(strlen(directory) + 7);
	strcpy(command, "mkdir ");
	strcat(command, directory);
	//cout << command << endl;
	system(command);
	return directory;
}

void truncate_segment(rvm_t rvm, const char* segname)
{
	//Truncates the segname log file in rvm_t directory

	string fullpath = get_full_file_path(rvm,segname);
	string seglog = string(segname) + ".log";
	string buf;
	int offset;
	char *data, *token;
	string command = "rm " + get_full_file_path(rvm,segname) + ".log";
	if (fileSize(rvm, (char*) seglog.c_str()) > -1)
	{
		ifstream logfile((char*) (fullpath+".log").c_str());
		if (logfile.is_open())
		{
			while (getline(logfile, buf))
			{
				token = strtok((char *) buf.c_str(), "|");
				offset = atoi(token);
				token = strtok(NULL, "|");
				data = token;
				//cout<<"\nOffset: "<<offset<<"\nData: "<<data<<endl;

				FILE * segFile;
				segFile = fopen((char *) fullpath.c_str(), "r+");
				fseek(segFile, offset, SEEK_SET);
				fwrite(data, sizeof(char),(string(data)).length(),segFile);
				fclose(segFile);
			}
			logfile.close();
		}
		system((char *) command.c_str());
	}
	else
	{
		system((char *) command.c_str());
	}
}

void show_segments()
{
	map<const char*, struct seg_entry>::iterator iter;
	iter = segments.begin();
	//cout << "show segments";
	//cout << endl << "size" << segments.size();
	while (iter != segments.end())
	{
		//cout << endl;
		cout << "Key: " << iter->first << endl << "Values:" << endl;
		cout << iter->second.size << endl << "address";
		printf("\n Address %lu\n", (iter->second.address));
		if (iter->second.address != NULL)
			cout << "Value at address" << string(iter->second.address) << endl;
		cout << "Mapped" << iter->second.is_mapped << endl;
		cout << "In use" << iter->second.in_use << endl;
		iter++;
	}

}

void show_segaddresstoname()
{
	//cout << endl << "Show seg address to name: size " << segaddresstoname.size();
	map<void*, const char*>::iterator iter;
	iter = segaddresstoname.begin();
	while (iter != segaddresstoname.end())
	{
		cout << endl;
		cout << "Key: " << iter->first << endl << "Values:" << endl;
		cout << iter->second;
		iter++;
	}

}

void display_undo_recs(){
	map<trans_t, vector<struct undo_record *> > :: iterator it;
	vector<undo_record*>::iterator vit;
	cout << endl << "DISPLAY UNDO RECS" <<endl;
	for(it = undo_log.begin(); it!= undo_log.end(); it++){
		vector<undo_record*> undo_recs = it -> second;
		//for(vit = undo_recs.begin(); vit!=undo_recs.end(); vit++){
		//printf("\nSegname: %s" , *vit->data);
		//printf("\nData" , **vit->data );
		//}
		for(long i=0; i< undo_recs.size(); i++){
			printf("%s", undo_recs[i] -> segname);
			printf("%s", undo_recs[i] -> data);
			printf("%d", undo_recs[i] -> offset);
			cout<<endl;
		}
	}
}

void rvm_unmap(rvm_t rvm, void*segbase)
{
	const char * segname = segaddresstoname[segbase];
	//cout << "Unmapping segment name " << segname;
	//const char* del = (const char *)segbase;
	//delete del;
	segments[segname].address = NULL;
	segments[segname].is_mapped = 0;
	//show_segments();
}

void rvm_destroy(rvm_t rvm, const char* segname)
{
	if (segments[segname].is_mapped == 1)
	{
		cout << "The segment is mapped. Cannot delete.";
		return;
	}

	//Deleting from hash map
	map<const char*, struct seg_entry>::iterator deletei = segments.find(
			segname);
	segments.erase(deletei);
	string deleted_name = get_full_file_path(rvm, segname);

	char* command = (char*) malloc(strlen(deleted_name.c_str()) + 4);
	strcpy(command, "rm ");
	strcat(command, deleted_name.c_str());
	//cout << command;
	system(command);
	//show_segments();
}

void *rvm_map(rvm_t rvm, const char *segname, int size_to_create)
{
	int file_length;
	struct seg_entry *segment = new seg_entry;
	char * vm_address;
	char * buffer;

	//Construct file full path
	string dir(rvm);
	string fil(segname);
	string separator = "/";
	string full_path = dir + separator + fil;

	file_length = fileSize(rvm, segname);

	if (file_length > -1)
	{
		//cout << "Segment  exists on disk." << endl;
		//Segment exists on disk
		//Check if map is called only on unmapped segment
		//First check if segment itself exists in hash map
		if (segments.find(segname) != segments.end())
		{
			//cout << "Segment exists in segment table." << endl;
			if (segments[segname].is_mapped == 1)
			{
				//cout << "Segment is mapped." << endl;
				return NULL;
			}
			else
			{

				//cout << "Segment is not mapped." << endl;
				//Now reallocate memory for segment and map it in hash map.
				segments[segname].address = (char*) malloc(size_to_create);
				segments[segname].size = size_to_create;
				segments[segname].is_mapped = 1;
				segments[segname].in_use = 0;

			}
		}
		else
		{
			//Segment is not on hash map
			//Check if more size requirements, in which case it is also unmapped
			if (file_length < size_to_create)
			{

				cout << "File is of smaller size. Reallocating :" << endl;
				segments[segname].address = (char*) realloc(
						segments[segname].address, size_to_create);
				segments[segname].size = size_to_create;
				segments[segname].is_mapped = 1;
				segments[segname].in_use = 0;

			}
			else
			{
				//Insert in hash map again
				segment->in_use = 0;
				segment->is_mapped = 1;
				segment->address = (char*) malloc(size_to_create);
				segment->size = size_to_create;
				segments.insert(
						pair<const char*, struct seg_entry>(segname, *segment));

			}

		}
		//Put entry in map that gives segment name from segment address
		vm_address = segments[segname].address;


		//In any case, I need to read segment from disk and apply changes in its redo log.
		truncate_segment(rvm, segname);
		ifstream infile((const char*) full_path.c_str());
		if(!infile.is_open()){
			printf("\nERROR could not open file");
			return NULL;
		}
		file_length = fileSize(rvm, segname);
		buffer = new char[file_length + 1];
		//memset(segments[segname].address,'\0',size_to_create);
		infile.read(buffer, file_length);
		memcpy(segments[segname].address, buffer, file_length);

	}
	else
	{
		//Segment does not exist
		//Create segment on disk
		//cout << "segment does not exist";

		ofstream outfile((const char*) full_path.c_str());
		outfile.close();
		//Create corresponding entry in VM
		vm_address = (char*) malloc(size_to_create);
		//memset(vm_address,'\0',size_to_create);
		//Create mapping in hash map of segments
		//Create new segment

		segment->in_use = 0;
		segment->is_mapped = 1;
		segment->address = vm_address;
		segment->size = size_to_create;
		segments.insert(pair<const char*, struct seg_entry>(segname, *segment));
	}
	segaddresstoname[vm_address] = segname;
	//show_segments();
	//show_segaddresstoname();
	return segments[segname].address;
}

trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases)
{
	string segs[numsegs];
	int i;
	map<const char*, struct seg_entry>::iterator iter;
	for (i = 0; i < numsegs; i++)
	{
		void * p = segbases[i];
		//show_segaddresstoname();
		//printf("p is %lu", p);
		char *segname = (char *) segaddresstoname[p];
		printf("\nTransaction to be done for segment %s", segname);
		segs[i] = string(segname);
		if (segments.find(segname) == segments.end())
		{
			cout << "Segment " << segs[i] << " has not been mapped" << endl;
			return -1;
		}
		//show_segments();
		iter = segments.find(segname);
		if(iter == segments.end())
			cout << "Not found in map" <<endl;
		//cout <<"Iter:"<< iter -> second.in_use<<endl;
		if (segments[segname].in_use == 1)
		{
			cout << "Segment " << segs[i]<< " is being used by another transaction" << endl;
			return -1;
		}
		else
		{
			//segments[segname].in_use = 1;
		}
	}
	trans_count++;
	trans_rvm[trans_count] = rvm;
	return trans_count;
}

void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size)
{
	struct redo_record *r = new redo_record();
	vector<struct redo_record *> redo_recs;
	vector<struct undo_record *> undo_recs;

	if(trans_rvm.find(tid)!=trans_rvm.end())
	{
		//Creating undo record
		undo_record *u = new undo_record();
		char *segname = (char *) segaddresstoname[segbase];
		//u={segname,offset,size,(char *)segbase};
		u->segname = segname;
		u->offset = offset;
		u->size = size;
		u->data = (char *)malloc(size);
		memcpy(u->data, (char *) segbase + offset, size );
		//u->data = (char *) ((char *) segbase + offset);

		if (undo_log.find(tid) != undo_log.end())
		{
			undo_recs = undo_log[tid];
			undo_recs.push_back(u);
			undo_log[tid] = undo_recs;

		}

		else
		{
			undo_recs.push_back(u);
			undo_log[tid] = undo_recs;
		}

		/*cout << "UNDO LOG" <<endl;

		for(long i=0; i< undo_recs.size(); i++){
			printf("%s", undo_recs.at(i) -> segname);
			printf("%s", undo_recs.at(i) -> data);
			cout<<endl;
		}*/

		if (redo_log.find(tid) != redo_log.end())
		{
			redo_recs = redo_log[tid];
			r->segname = segname;
			r->offset = offset;

			redo_recs.push_back(r);
			redo_log[tid] = redo_recs;

		}

		else
		{
			r->segname = segname;
			r->offset = offset;

			redo_recs.push_back(r);
			redo_log[tid] = redo_recs;
		}
	}
	else
	{
		cout<<"\nInvalid Transaction ID"<<endl;
	}
}

void rvm_commit_trans(trans_t tid)
{
	//cout << "\nInside commit transaction" << endl;

	if (redo_log.find(tid) != redo_log.end())
	{
		vector<redo_record*> redo_recs;
		redo_recs = redo_log[tid];
		for (vector<redo_record *>::iterator it = redo_recs.begin();
				it != redo_recs.end(); ++it)
		{
			redo_record *r = new redo_record();
			r = *it;
			char *segname = (char *) (r->segname);
			r->new_data = (char *) (segments[segname].address + r->offset);
			string seglog = get_full_file_path(trans_rvm[tid],segname)+".log";
			//cout << "Creating log file in commit transaction at " << seglog;
			ofstream segfile((char *) seglog.c_str(), ios::out | ios::app);
			//segfile.open ((char *)seglog.c_str(), ios::out | ios::app);
			segfile << r->offset << "|" << r->new_data << endl;
			segfile.close();
		}
		undo_log.erase(tid);
		redo_log.erase(tid);
		trans_rvm.erase(tid);
	}
	else
	{
		cout << "\nIncorrect Transaction ID" << endl;
	}
}

void rvm_abort_trans(trans_t tid)
{
	if (undo_log.find(tid) != undo_log.end())
	{
		vector<undo_record*> undo_recs;
		undo_recs = undo_log[tid];
		undo_record *u = new undo_record();
		//cout<<"\nInside Abort:"<<endl;
		//display_undo_recs();
		for (vector<undo_record*>::iterator it = undo_recs.begin();
				it != undo_recs.end(); ++it)
		{
			u = *it;
			char *segname = (char *) u->segname;
			char *data = u->data;
			memcpy(segments[segname].address + u->offset, data, u->size);
			//free (u);
		}
		undo_log.erase(tid);
		redo_log.erase(tid);
		trans_rvm.erase(tid);
	}
	else
	{
		cout << "\nIncorrect Transaction ID" << endl;
	}
}

void rvm_truncate_log(rvm_t rvm)
{
	DIR *d;
	struct dirent *dir;
	vector<string> dirlist;
	d = opendir(("./"+string(rvm)).c_str());
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			string fname(dir->d_name);
			if (fname.find(".log") != string::npos)
			{
				dirlist.push_back(dir->d_name);
			}

		}
		closedir(d);
	}
	for (vector<string>::iterator it = dirlist.begin(); it != dirlist.end();
			++it)
	{
		string s = *it;
		s = s.substr(0, s.length() - 4);
		char *segname = (char *) s.c_str();
		truncate_segment(rvm, segname);
	}
}
