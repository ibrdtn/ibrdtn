/*
 * TarUtils.cpp
 *
 * Copyright (C) 2013 IBR, TU Braunschweig
 *
 * Written-by: David Goltzsche <goltzsch@ibr.cs.tu-bs.de>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *  Created on: Sep 16, 2013
 */

#include "TarUtils.h"
#include <stdlib.h>
#include <string.h>

#define BUFF_SIZE 8192

std::string TarUtils::_img_path = "";
std::string TarUtils::_outbox_path = "";
int TarUtils::ret = 0;

#ifdef HAVE_LIBTFFS
tffs_handle_t TarUtils::htffs = 0;
tdir_handle_t TarUtils::hdir = 0;
tfile_handle_t TarUtils::hfile = 0;
#endif

#ifdef HAVE_LIBARCHIVE
TarUtils::TarUtils()
{
}
TarUtils::~TarUtils()
{
}

int TarUtils::open_callback( struct archive *, void *blob_iostream )
{
	//blob does not need to be opened, do nothing
	return ARCHIVE_OK;
}

ssize_t TarUtils::write_callback( struct archive *, void *blob_ptr, const void *buffer, size_t length )
{
	char* cast_buf = (char*) buffer;

	ibrcommon::BLOB::Reference blob = *((ibrcommon::BLOB::Reference*) blob_ptr);
	ibrcommon::BLOB::iostream os = blob.iostream();

	(*os).write(cast_buf, length);

	return length;
}



ssize_t TarUtils::read_callback( struct archive *a, void *blob_ptr, const void **buffer )
{
	char *cbuff = new char[BUFF_SIZE];

	ibrcommon::BLOB::Reference *blob = (ibrcommon::BLOB::Reference*) blob_ptr;
	ibrcommon::BLOB::iostream is = blob->iostream();

	(*is).read(cbuff,BUFF_SIZE);

	*buffer = cbuff;
	return BUFF_SIZE;
}

int TarUtils::close_callback( struct archive *, void *blob_iostream )
{
	//blob does not need to be closed, do nothing
	return ARCHIVE_OK;
}

void TarUtils::read_tar_archive( string extract_folder, ibrcommon::BLOB::Reference *blob )
{
	struct archive *a;
	struct archive_entry *entry;
	int ret,fd;


	a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_compression_all(a);
	archive_read_support_format_tar(a);

	archive_read_open(a, (void*) blob, &open_callback, &read_callback, &close_callback);


	while ((ret = archive_read_next_header(a, &entry)) == ARCHIVE_OK )
	{
		string filename = archive_entry_pathname(entry);
		string path = extract_folder + "/" + filename;
		string dirs = dir_path(path);
		mkdir(dirs.c_str(),0777);
		fd = open(path.c_str(),O_CREAT|O_WRONLY,0666);
		if(fd < 0)
		{
			cout << "ERROR: cannot open file " << path << endl;
			return;
		}
		archive_read_data_into_fd(a,fd);
		close(fd);
	}

	archive_read_free(a);
}

void TarUtils::write_tar_archive( ibrcommon::BLOB::Reference *blob, list<ObservedFile*> files_to_send)
{

	char buff[BUFF_SIZE];
	size_t len;
	int fd;
	//create new archive, set format to tar, use callbacks (above this method)
	struct archive *a;
	a = archive_write_new();
	archive_write_set_format_ustar(a);
	archive_write_open(a, blob, &open_callback, &write_callback, &close_callback);

	list<ObservedFile*>::iterator of_iter;
	for(of_iter = files_to_send.begin(); of_iter != files_to_send.end(); of_iter++)
	{
		ObservedFile* of = (*of_iter);

		string filename = of->getPath();
		struct archive_entry *entry;
		entry= archive_entry_new();
		archive_entry_set_size(entry, of->size());
		if(of->isDirectory())
		{
			archive_entry_set_filetype(entry, AE_IFDIR);
			archive_entry_set_perm(entry, 0755);
		}
		else
		{
			archive_entry_set_filetype(entry, AE_IFREG );
			archive_entry_set_perm(entry, 0644);
		}

		archive_entry_set_pathname(entry, rel_filename(of->getPath()).c_str());

		//set timestamps
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		archive_entry_set_atime(entry, ts.tv_sec, ts.tv_nsec); //accesstime
		archive_entry_set_birthtime(entry, ts.tv_sec, ts.tv_nsec); //creationtime
		archive_entry_set_ctime(entry, ts.tv_sec, ts.tv_nsec); //time, inode changed
		archive_entry_set_mtime(entry, ts.tv_sec, ts.tv_nsec); //modification time

		archive_write_header(a,entry);
		//read normal file
		if(_img_path == "")
		{
			fd = open(filename.c_str(), O_RDONLY);
			len = read(fd, buff, BUFF_SIZE);
		}
#ifdef HAVE_LIBTFFS
		//read file on vfat-image
		else
		{
			//mount tffs
			byte* path = const_cast<char *>(_img_path.c_str());
			if ((ret = TFFS_mount(path, &htffs)) != TFFS_OK)
			{
				cout << "ERROR: TFFS_mount" << ret << endl;
				return;
			}

			//open file
			byte* file = const_cast<char*>(filename.c_str());
			if ((ret = TFFS_fopen(htffs, file, "r", &hfile)) != TFFS_OK)
			{
				cout << "ERROR: TFFS_fopen" << ret << filename << endl;
				return;
			}

			memset(buff, 0, BUFF_SIZE);
			//read file
			if (( ret = TFFS_fread(hfile,BUFF_SIZE,(unsigned char*) buff)) < 0)
			{
					if( ret == ERR_TFFS_FILE_EOF)
						len = 0;

					else
					{
						cout << "ERROR: TFFS_fread" << ret << endl;
						return;
					}

			}
			else
			{
					len = ret;
			}
		}
#endif
		//write buffer to archive
		while (len > 0)
		{
			if( (ret = archive_write_data(a, buff, len)) < 0)
			{
				cout << "ERROR: archive_write_data " << ret << endl;
				return;
			}
			if(_img_path == "")
				len = read(fd, buff, sizeof(buff));
#ifdef HAVE_LIBTFFS
			else
			{
				if (( ret  = TFFS_fread(hfile,sizeof(buff),(unsigned char*) buff)) < 0)
				{
					if( ret == ERR_TFFS_FILE_EOF)
					{
						len = 0;
						continue;
					}
					cout << "ERROR: TFFS_fread" << ret << endl;
					return;
				}
				len = ret;
			}
#endif HAVE_LIBTFFS

		}

		//close file
		if(_img_path == "")
			close(fd);
#ifdef HAVE_LIBTFFS
		else
		{
			if ((ret = TFFS_fclose(hfile)) != TFFS_OK) {
				cout << "ERROR: TFFS_fclose" << ret;
				return;
			}
		}
#endif
		archive_entry_free(entry);
	}
	archive_write_close(a);
	archive_write_free(a);

}

void TarUtils::set_img_path( std::string img_path )
{
	_img_path = img_path;
}

void TarUtils::set_outbox_path( std::string outbox_path )
{
	_outbox_path = outbox_path;
}

std::string TarUtils::rel_filename(std::string n)
{
	return n.substr(_outbox_path.length()+1,n.length()-_outbox_path.length()-1);
}

std::string TarUtils::dir_path(std::string p)
{
	unsigned slash_pos = p.find_last_of('/', p.length());
	return p.substr(0, slash_pos);
}

#endif