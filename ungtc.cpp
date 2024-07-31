/*
UNGTC - ATD archive tool.

Copyright (C) 2013 JrMasterModelBuilder

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cstring>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>

#ifdef _WIN32
	#include <direct.h>

	const char dir_sep = '\\';
#else
	const char dir_sep = '/';
#endif

using namespace std;

struct FileData
{
	string path;
	uint32_t size;
	uint32_t posi;
	char *data;
	FileData()
	{
		size = 0;
		posi = 0;
	}
	~FileData()
	{
	}
};

struct FileArchive
{
	string path;
	uint32_t offset;
	uint32_t size;
	FileArchive()
	{
		size = 0;
		offset = 0;
	}
	~FileArchive()
	{
	}
};

struct CompressBlocks
{
	uint32_t offset;
	uint32_t block;
	uint32_t size;
	CompressBlocks()
	{
		offset = 0;
		block = 0;
		size = 0;
	}
	~CompressBlocks()
	{
	}
};

fstream fs;
string config_file = ".GTC";
uint32_t uncompressed_block_size = 131072;

bool read_file(FileData *file)
{
	//Open the file with read permissions.
	fs.open(file->path.c_str(), fstream::in|fstream::binary);
	
	//Check if file opened successfully.
	if(!fs)
	{
		//Return error.
		fs.close();
		return false;
	}
	
	//Find the size of the file.
	fs.seekg(0, fs.end);
	file->size = fs.tellg();
	fs.seekg(0, fs.beg);
	
	//Read the file into memory.
	if(file->size)
	{
		file->data = new char [file->size];
		fs.read(file->data, file->size);
	}
	
	//Close the file.
	fs.close();
	
	//Return successful.
	return true;
}

bool write_file(FileData *file)
{
	//Open the file with write permissions.
	fs.open(file->path.c_str(), fstream::out|fstream::binary);
	
	//Check if file opened successfully.
	if(!fs)
	{
		//Return error.
		fs.close();
		return false;
	}
	
	//Write the file.
	fs.write(file->data, file->size);
	
	//Close the file.
	fs.close();
	
	//Return successful.
	return true;
}

bool is_directory(string path)
{
	bool exists = false;
	#ifdef _WIN32
		struct _stat sb;
		exists = _stat(path.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode);
	#else
		struct stat sb;
		exists = stat(path.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode);
	#endif
	return exists;
}

bool make_dir(string path)
{
	bool success = false;
	#ifdef _WIN32
		success = _mkdir(path.c_str()) == 0;
	#else
		success = mkdir(path.c_str(), 0755) == 0;
	#endif
	return success;
}

uint32_t copy_data(FileData *from, FileData *to, uint32_t bytes, int32_t from_offset = 0, int32_t to_offset = 0)
{
	memcpy(to->data + to->posi + to_offset, from->data + from->posi + from_offset, bytes);
	return bytes;
}

void directory_listing_recursize(string folder_path, string relative_path, vector<string> * list)
{
	DIR * dir;
	dirent * entry;
	struct stat entry_stat;
	dir = opendir(folder_path.c_str());
	if(dir)
	{
		while(entry = readdir(dir))
		{
			string name = entry->d_name;
			//Ignore system files.
			if(name[0] == '.')
			{
				continue;
			}
			//If entry type detectiong fails, skip it.
			if(stat((folder_path + name).c_str(), &entry_stat) < 0)
			{
				continue;
			}
			if(S_ISDIR(entry_stat.st_mode))
			{
				directory_listing_recursize(folder_path + name + dir_sep, relative_path + name + dir_sep, list);
			}
			else
			{
				list->push_back(relative_path + name);
			}
		}
	}
	closedir(dir);
}

bool extract(string folder_path)
{
	FileData compress_inf,
		filelist_inf,
		gamedata_gtc,
		gamedata
	;
	string extract_path,
		extract_folder
	;
	vector<string> extract_folders;
	vector<FileArchive> filelist_files;
	vector<CompressBlocks> compress_blocks;
	uint32_t format_version = 0,
		compress_block_size = 0
	;
		
	extract_folder = "GAMEDATA";
	extract_path = folder_path + extract_folder + dir_sep;
	
	//Check that the extraction folder does not exist.
	if(is_directory(extract_path))
	{
		cout << "ERROR: Extraction folder already exists: " << extract_folder << dir_sep << endl;
		return false;
	}
	
	//Set the paths to the archive files.
	compress_inf.path = folder_path + "COMPRESS.INF";
	filelist_inf.path = folder_path + "FILELIST.INF";
	gamedata_gtc.path = folder_path + "GAMEDATA.GTC";
	
	//Load them all into memory.
	cout << "Loading: COMPRESS.INF" << endl;
	if(!read_file(&compress_inf))
	{
		cout << "ERROR: Failed to open: COMPRESS.INF" << endl;
		return false;
	}
	cout << "Loaded: " << compress_inf.size << " bytes." << endl;
	
	cout << "Loading: FILELIST.INF" << endl;
	if(!read_file(&filelist_inf))
	{
		cout << "ERROR: Failed to open: FILELIST.INF" << endl;
		return false;
	}
	cout << "Loaded: " << filelist_inf.size << " bytes." << endl;
	
	cout << "Loading: GAMEDATA.GTC" << endl;
	if(!read_file(&gamedata_gtc))
	{
		cout << "ERROR: Failed to open: GAMEDATA.GTC" << endl;
		return false;
	}
	cout << "Loaded: " << gamedata_gtc.size << " bytes." << endl;
	
	cout << "Creating: Extraction folder: " << extract_folder << dir_sep << endl;
	
	if(!make_dir(extract_path))
	{
		cout << "ERROR: Failed to create extraction folder: " << extract_folder << dir_sep << endl;
		return false;
	}
	
	//Detect the file format type by detecting how COMPRESS.INF is structured.
	//Check after how many bytes the next block continued from the first.
	//Also detecting the possibility within the format of a single block.
	if( (compress_inf.size >= 16
		&& compress_inf.data[ 8] == compress_inf.data[12]
		&& compress_inf.data[ 9] == compress_inf.data[13]
		&& compress_inf.data[10] == compress_inf.data[14]
		&& compress_inf.data[11] == compress_inf.data[15]
		) || compress_inf.size == 12 )
	{
		format_version = 2;
	}
	else if( (compress_inf.size >= 24
		&& compress_inf.data[ 8] == compress_inf.data[20]
		&& compress_inf.data[ 9] == compress_inf.data[21]
		&& compress_inf.data[10] == compress_inf.data[22]
		&& compress_inf.data[11] == compress_inf.data[23]
		) || compress_inf.size == 20 )
	{
		format_version = 1;
	}
	
	if(format_version)
	{
		cout << "Detected: File format version: " << format_version << endl;
		switch(format_version)
		{
			case 1:
				compress_block_size = 20;
			break;
			case 2:
				compress_block_size = 12;
			break;
		}
	}
	else
	{
		cout << "ERROR: Failed to detect file format version." << endl;
		return false;
	}
	
	cout << "Reading: FILELIST.INF" << endl;
	for(uint32_t i = 0; i+136 <= filelist_inf.size; i += 136)
	{
		FileArchive f;
		f.path = string(&filelist_inf.data[i], 128);
		f.offset = ( ((unsigned char)filelist_inf.data[i+128])
			| (((unsigned char)filelist_inf.data[i+129]) << 8)
			| (((unsigned char)filelist_inf.data[i+130]) << 16)
			| (((unsigned char)filelist_inf.data[i+131]) << 24) )
		;
		f.size = ( ((unsigned char)filelist_inf.data[i+132])
			| (((unsigned char)filelist_inf.data[i+133]) << 8)
			| (((unsigned char)filelist_inf.data[i+134]) << 16)
			| (((unsigned char)filelist_inf.data[i+135]) << 24) )
		;
		filelist_files.push_back(f);
		
		//Read each folder in the file path until there are not more backslashes.
		for(size_t pos = 0, path_size = f.path.size(); pos < path_size;)
		{
			size_t pos_end = f.path.find_first_of('\\', pos + 1);
			if(pos_end < string::npos)
			{
				string path = f.path.substr(0, pos_end);
				//If this path has not been added to the list of folder to extract to, add it.
				if(!(std::find(extract_folders.begin(), extract_folders.end(), path) != extract_folders.end()))
				{
					extract_folders.push_back(path);
				}
			}
			pos = pos_end;
		}
	}
	cout << "Read: " << filelist_inf.size << " bytes, " << filelist_files.size() << " entries." << endl;
	delete[] filelist_inf.data;
	
	cout << "Reading: COMPRESS.INF" << endl;
	for(uint32_t i = 0; i+compress_block_size <= compress_inf.size; i += compress_block_size)
	{
		CompressBlocks b;
		b.offset = ( ((unsigned char)compress_inf.data[i])
			| (((unsigned char)compress_inf.data[i+ 1]) << 8)
			| (((unsigned char)compress_inf.data[i+ 2]) << 16)
			| (((unsigned char)compress_inf.data[i+ 3]) << 24) )
		;
		b.block = ( (unsigned char)(compress_inf.data[i+4])
			| (((unsigned char)compress_inf.data[i+ 5]) << 8)
			| (((unsigned char)compress_inf.data[i+ 6]) << 16)
			| (((unsigned char)compress_inf.data[i+ 7]) << 24) )
		;
		b.size = ( ((unsigned char)compress_inf.data[i+8])
			| (((unsigned char)compress_inf.data[i+ 9]) << 8)
			| (((unsigned char)compress_inf.data[i+10]) << 16)
			| (((unsigned char)compress_inf.data[i+11]) << 24) )
		;
		compress_blocks.push_back(b);
	}
	cout << "Read: " << compress_inf.size << " bytes, " << compress_blocks.size() << " entries." << endl;
	delete[] compress_inf.data;
	
	//Allocate memory to decompress the block into.
	gamedata.size = compress_blocks.size() * uncompressed_block_size;
	gamedata.data = new char [gamedata.size];
	cout << "Decompressing: GAMEDATA.GTC to " << gamedata.size << " bytes." << endl;
	
	//Read the compress block.
	for(uint32_t i = 0, il = compress_blocks.size(); i < il; ++i)
	{
		CompressBlocks b = compress_blocks[i];
		uint32_t end_of_block = b.offset + b.size;
		
		cout << "Block: " << i+1 << "/" << il << endl;
		
		//Update our position in the decompressed file.
		gamedata.posi = i * uncompressed_block_size;
		
		//Update our position in the compressed file.
		gamedata_gtc.posi = end_of_block - b.block;
		
		//Check if this block is uncompressed.
		if(b.size == uncompressed_block_size)
		{
			//This block is uncompressed, copy as is.
			gamedata.posi += copy_data(&gamedata_gtc, &gamedata, b.size);
		}
		else
		{
			bool back_ref = false;
			uint32_t single = 0,
				back_ref_size = 0
			;
			int back_ref_jump = 0;
			
			do
			{
				//Copy the command bits and move past.
				uint32_t command = ((unsigned char)gamedata_gtc.data[gamedata_gtc.posi])
					| (((unsigned char)gamedata_gtc.data[gamedata_gtc.posi+1]) << 8)
					| (((unsigned char)gamedata_gtc.data[gamedata_gtc.posi+2]) << 16)
					| (((unsigned char)gamedata_gtc.data[gamedata_gtc.posi+3]) << 24)
				;
				gamedata_gtc.posi += 4;
				
				//Read the bits from right to left, but only until the entire block or file is read.
				for(uint32_t j = 0; j < 32 && gamedata_gtc.posi < end_of_block && gamedata.posi < gamedata.size; ++j)
				{
					//Get the boolean value of the bit.
					bool bit = (command >> j & 1);
					
					//Check if already in a backref.
					if(back_ref)
					{
						//Check if we have detected what kind of backref yet.
						if(single)
						{
							//We are in a single byte backref, we need to read the size.
							
							//Add the value of the bit if set.
							if(bit)
							{
								//This addition logic only works because we never go above 2 bits
								back_ref_size += single;
							}
							
							//If we have read the last bit, copy the bytes and exit the backref.
							if(!--single)
							{
								//We know how big the backref is now, copy the data from the decompressed data from the specified backref jump.
								for(uint32_t k = 0; k < back_ref_size; ++k)
								{
									gamedata.posi += copy_data(&gamedata, &gamedata, 1, back_ref_jump);
								}
								
								back_ref = false;
							}
						}
						else
						{
							//We have not yet determined what kind of backref this is.
							if(bit)
							{
								//A self-contained multi-byte, read it now and continue on.
								
								//Read the first 3 bits.
								back_ref_size = ((unsigned char)gamedata_gtc.data[gamedata_gtc.posi]) & 0x07;
								
								//Read the block offset.
								back_ref_jump = -8192 + ( ((((unsigned char)gamedata_gtc.data[gamedata_gtc.posi]) & 0xF8) >> 3)
														 | (((unsigned char)gamedata_gtc.data[gamedata_gtc.posi+1]) << 5) )
								;
								
								gamedata_gtc.posi += 2;
								
								//If all the bits are 0, this is a 3 byte block, else it is a 2 byte block.
								if(!back_ref_size)
								{
									//Read the 7 bits that are the backref block size.
									back_ref_size = ((unsigned char)gamedata_gtc.data[gamedata_gtc.posi]) & 0x7F;
									
									//Check the last 1 bit to see if the backref starts 2x further back.
									if( ((unsigned char)gamedata_gtc.data[gamedata_gtc.posi]) & 0x80 )
									{
										back_ref_jump -= 8192;
									}
									
									gamedata_gtc.posi++;
									
									//If the backref size is still 0, read the next 2 bytes to get the size.
									if(!back_ref_size)
									{
										back_ref_size = (((unsigned char)gamedata_gtc.data[gamedata_gtc.posi]))
										| (((unsigned char)gamedata_gtc.data[gamedata_gtc.posi+1]) << 8)
										;
										gamedata_gtc.posi += 2;
									}
									else
									{
										//The minimum size of a backref is 2, so add 2 to the total.
										back_ref_size += 2;
									}
								}
								else
								{
									//The minimum size of a backref is 2, so add 2 to the total.
									back_ref_size += 2;
								}
								
								//Copy the data and more ahead.
								for(uint32_t k = 0; k < back_ref_size; ++k)
								{
									gamedata.posi += copy_data(&gamedata, &gamedata, 1, back_ref_jump);
								}
								
								back_ref = false;
							}
							else
							{
								//This is a single byte backref, we need to read the next 2 bits.
								single = 2;
								//The minimum single backref size is 2.
								back_ref_size = 2;
								
								//We do not know how large the backref block is yet, but we must read where, in case the size runs into the next command block.
								back_ref_jump = -256 + ((unsigned char)gamedata_gtc.data[gamedata_gtc.posi]);
								
								gamedata_gtc.posi++;
							}
						}
					}
					else
					{
						//Not currently in a backref, check if literal or declaring a backref.
						if(bit)
						{
							//Backref, setup to read what kind from the next bit(s).
							back_ref = true;
							single = false;
						}
						else
						{
							//Literal byte, copy it.
							gamedata.posi += copy_data(&gamedata_gtc, &gamedata, 1);
							gamedata_gtc.posi++;
						}
					}
				}
			}
			while(gamedata_gtc.posi < end_of_block);
		}
	}
	
	cout << "Decompressed: GAMEDATA.GTC, writing files." << endl;
	
	cout << "Writing: " << config_file << endl;
	//Open the file with write permissions.
	fs.open((extract_path + config_file).c_str(), fstream::out);
	//Check if file opened successfully.
	if(!fs)
	{
		//Return error.
		fs.close();
		return false;
	}
	//Add the configuration info.
	fs << "version=" << format_version << ";" << endl;
	//Close the file.
	fs.close();
	
	//Create all the directories needed to extract to.
	for(uint32_t i = 0, il = extract_folders.size(); i < il; ++i)
	{
		string path = extract_folders[i];
		//Resolve slashes if necessary.
		if(dir_sep != '\\')
		{
			replace(path.begin(), path.end(), '\\', dir_sep);
		}
		path = extract_path + path + dir_sep;
		cout << "Creating: " << extract_folders[i] << endl;
		//Create the directory and check success.
		if(!make_dir(path))
		{
			cout << "ERROR: Failed to create directory: " << extract_folders[i] << endl;
			return false;
		}
	}
	
	//Extract all the file.
	for(uint32_t i = 0, il = filelist_files.size(); i < il; ++i)
	{
		FileArchive f = filelist_files[i];
		FileData file;
		string path = f.path;
		//Resolve slashes if necessary.
		if(dir_sep != '\\')
		{
			replace(path.begin(), path.end(), '\\', dir_sep);
		}
		file.path = extract_path + path;
		file.size = f.size;
		file.data = gamedata.data + f.offset;
		cout << "Writing: " << f.path << endl;
		if(!write_file(&file))
		{
			cout << "ERROR: Failed to write file: " << f.path << endl;
			return false;
		}
	}
	
	return true;
}

bool archive(string folder_path, uint32_t format_version)
{
	switch(format_version)
	{
		case 1:
		case 2:
			cout << "Creating: Archive version " << format_version << "." << endl;
		break;
		default:
			cout << "ERROR: Unrecognized archive version " << format_version << "." << endl;
			return false;
		break;
	}
	
	vector<string> file_list;
	vector<FileArchive> files;
	string archive_path = folder_path + ".." + dir_sep;
	fstream fs2;
	
	//Check if the archives already exist to prevent overwriting files.
	fs.open((archive_path + "COMPRESS.INF").c_str(), fstream::in);
	if(fs)
	{
		cout << "ERROR: Archive file already exists: " << archive_path << "COMPRESS.INF" << endl;
		fs.close();
		return false;
	}
	fs.open((archive_path + "FILELIST.INF").c_str(), fstream::in);
	if(fs)
	{
		cout << "ERROR: Archive file already exists: " << archive_path << "FILELIST.INF" << endl;
		fs.close();
		return false;
	}
	fs.open((archive_path + "GAMEDATA.GTC").c_str(), fstream::in);
	if(fs)
	{
		cout << "ERROR: Archive file already exists: " << archive_path << "GAMEDATA.GTC" << endl;
		fs.close();
		return false;
	}
	
	cout << "Reading: Finding files to archive." << endl;
	
	//Get the list of file recursively.
	directory_listing_recursize(folder_path, "", &file_list);
	
	cout << "Finding: Listing all files to archive." << endl;
	cout << "Found: " << file_list.size() << " files." << endl;
	
	cout << "Writing: " << archive_path << "GAMEDATA.GTC" << endl;
	fs2.open((archive_path + "GAMEDATA.GTC").c_str(), fstream::out|fstream::binary);
	if(!fs2)
	{
		cout << "ERROR: Failed to write: " << archive_path << "GAMEDATA.GTC" << endl;
		return false;
	}
	
	for(uint32_t i = 0, il = file_list.size(); i < il; ++i)
	{
		FileData f;
		FileArchive fa;
		
		//Skip if the path is too long.
		if(file_list[i].size() >= 128)
		{
			cout << "Skipping: " << file_list[i] << ", path >= 128 characters." << endl;
			continue;
		}
		cout << "Archiving: " << file_list[i] << endl;
		
		//Set the file path.
		f.path = folder_path + file_list[i];
		
		//Read the file.
		if(!read_file(&f))
		{
			cout << "Skipping: " << file_list[i] << ", failed to read file." << endl;
			continue;
		}
		
		//Set the archive file path.
		fa.path = file_list[i];
		//Resolve slashes if necessary.
		if(dir_sep != '\\')
		{
			replace(fa.path.begin(), fa.path.end(), dir_sep, '\\');
		}
		//Set the archive file size.
		fa.size = f.size;
		
		//Write the file to the archive.
		if(f.size > 0)
		{
			//Save the offset withing the archive.
			fa.offset = fs2.tellg();
			//Write the file to the archive.
			fs2.write(f.data, f.size);
			delete[] f.data;
		}
		//Add the archive file to the list.
		files.push_back(fa);
	}
	uint32_t current_size,
		target_size,
		padding_size,
		compress_block_count
	;
	char *padding;
	//Get the current block size.
	current_size = fs2.tellg();
	//Find the target size and the number of compress blocks it takes.
	target_size = 0;
	compress_block_count = 0;
	while(target_size < current_size)
	{
		target_size += uncompressed_block_size;
		++compress_block_count;
	}
	//Increase the file size to the target size.
	padding_size = target_size - current_size;
	padding = new char[padding_size];
	memset(padding, 0, padding_size);
	fs2.write(padding, padding_size);
	delete[] padding;
	cout << "Wrote: " << fs2.tellg() << " bytes." << endl;
	fs2.close();

	//Write the file list.
	cout << "Writing: " << archive_path << "FILELIST.INF" << endl;
	fs2.open((archive_path + "FILELIST.INF").c_str(), fstream::out|fstream::binary);
	if(!fs2)
	{
		cout << "ERROR: Failed to write: " << archive_path << "FILELIST.INF" << endl;
		return false;
	}
	for(uint32_t i = 0, il = files.size(); i < il; ++i)
	{
		FileArchive f;
		f = files[i];
		
		//Copy the string into a fixed size char array to write to file.
		char file_archive_path[128];
		strncpy(file_archive_path, f.path.c_str(), 128);
		fs2.write(file_archive_path, 128);
		
		//Write the offset.
		char offset[4];
		offset[3] = f.offset >> 24;
		offset[2] = f.offset >> 16;
		offset[1] = f.offset >> 8;
		offset[0] = f.offset;
		fs2.write(offset, 4);
		
		//Write the size.
		char size[4];
		size[3] = f.size >> 24;
		size[2] = f.size >> 16;
		size[1] = f.size >> 8;
		size[0] = f.size;
		fs2.write(size, 4);
	}
	cout << "Wrote: " << fs2.tellg() << " bytes." << endl;
	fs2.close();
	
	//Create the reused byte blocks for the compress file.
	char dummy_uint32_char[4] = {0,0,0,0};
	char uncompressed_block_size_char[4];
	uncompressed_block_size_char[3] = uncompressed_block_size >> 24;
	uncompressed_block_size_char[2] = uncompressed_block_size >> 16;
	uncompressed_block_size_char[1] = uncompressed_block_size >> 8;
	uncompressed_block_size_char[0] = uncompressed_block_size;
	
	//Write the compress list.
	cout << "Writing: " << archive_path << "COMPRESS.INF" << endl;
	fs2.open((archive_path + "COMPRESS.INF").c_str(), fstream::out|fstream::binary);
	if(!fs2)
	{
		cout << "ERROR: Failed to write: " << archive_path << "COMPRESS.INF" << endl;
		return false;
	}
	for(uint32_t i = 0; i < compress_block_count; ++i)
	{
		//Write the offset.
		uint32_t offset = i * uncompressed_block_size;
		char offset_char[4];
		offset_char[3] = offset >> 24;
		offset_char[2] = offset >> 16;
		offset_char[1] = offset >> 8;
		offset_char[0] = offset;
		fs2.write(offset_char, 4);
		
		//Write the block size and compressed size.
		fs2.write(uncompressed_block_size_char, 4);
		fs2.write(uncompressed_block_size_char, 4);
		
		//Check if file format version 1.
		if(format_version == 1)
		{
			//Write the 2 irrelevant uint32 values that are ignored (possibly meaning compression ratio and the number of files that appear in the block).
			fs2.write(dummy_uint32_char, 4);
			fs2.write(dummy_uint32_char, 4);
		}
	}
	cout << "Wrote: " << fs2.tellg() << " bytes." << endl;
	fs2.close();
	
	return true;
}

int main(int argc, char *argv[])
{
	string folder_path;
	
	//Get the folder path from the arguments.
	if(argc > 1)
	{
		folder_path = argv[1];
		//Append trailing slash if not present.
		if(folder_path[folder_path.size()-1] != dir_sep)
		{
			folder_path += dir_sep;
		}
	}
	else
	{
		folder_path = '.';
		folder_path += dir_sep;
	}
	
	cout << "UNGTC - ATD archive tool." << endl << endl;
	cout << "Copyright (C) 2013 JrMasterModelBuilder" << endl << endl;
	cout << "This tool will extract ATD GTC archives and create compatible new ones." << endl << endl;
	cout << "USAGE: ungtc FILE_OR_FOLDER_PATH" << endl << endl;
	
	cout <<  "This program is free software: you can redistribute it and/or modify" << endl
		<< "it under the terms of the GNU General Public License as published by" << endl
		<< "the Free Software Foundation, either version 3 of the License, or" << endl
		<< "(at your option) any later version." << endl << endl
	;
	cout << "This program is distributed in the hope that it will be useful," << endl
		<< "but WITHOUT ANY WARRANTY; without even the implied warranty of" << endl
		<< "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the" << endl
		<< "GNU General Public License for more details." << endl << endl
	;
	cout << "You should have received a copy of the GNU General Public License" << endl
		<< "along with this program. If not, see <http://www.gnu.org/licenses/>." << endl << endl
	;
		
	cout << "Running UNGTC on: " << folder_path << endl;
	
	//Check if the config file exists for creating a GTC exists.
	fs.open((folder_path + config_file).c_str(), fstream::in);
	//Check if file opened successfully.
	if(fs)
	{
		uint32_t format_version = 0;
		string config_file_content;
		
		//The config file exists, create a GTC.
		cout << "Action: Archiving." << endl;
		
		//Read the contents of the config file into a string.
		fs >> config_file_content;
		
		fs.close();
		
		size_t pos,
			pos_end
		;
		
		pos = config_file_content.find_first_of("version=", 0) + 8;
		pos_end = config_file_content.find_first_of(";", pos);
		format_version = atoi(config_file_content.substr(pos, pos_end-pos).c_str());
		
		cout << (archive(folder_path, format_version) ? "COMPLETE: Archiving complete." : "ERROR: Archiving failed.") << endl;
	}
	else
	{
		//The config file does not exists, try to extract GTC.
		fs.close();
		
		cout << "Action: Extracting." << endl;
		
		cout << (extract(folder_path) ? "COMPLETE: Extraction complete." : "ERROR: Extraction failed.") << endl;
	}
	
	return 0;
}
