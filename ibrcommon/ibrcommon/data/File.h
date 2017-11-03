/*
 * File.h
 *
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
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
 */

#ifndef IBRCOMMON_FILE_H_
#define IBRCOMMON_FILE_H_

//#define USE_FILE_LOCKING 1

#include <iostream>
#include <map>
#include <vector>
#include <list>
#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "ibrcommon/Exceptions.h"


namespace ibrcommon
{
	/**
	 * A File object can hold a reference to any existing or non-existing
	 * files/directories. It provides a common set of file operations.
	 */
	class File
	{
	public:
		/**
		 * Instantiate a File object without a reference to a file.
		 */
		File();

		/**
		 * Instantiate a File object with a reference to a existing
		 * and non-existing file.
		 * @param path Filename or path to reference to.
		 */
		File(const std::string &path);

		/**
		 * Destructor of the file.
		 */
		virtual ~File();

		/**
		 * Returns the type of the file. The type is only updated when calling update().
		 * @see update()
		 * @return The filetype of the file (e.g. DT_REG, DT_LNK, DT_DIR, DT_UNKNOWN)
		 */
		unsigned char getType() const;

		/**
		 * Get all files in the directory. The given path in the constructor
		 * has to be a directory in this case.
		 * @param files A (empty) list to put the new file objects in.
		 * @return Returns zero on success and an error number on failure.
		 */
		int getFiles(std::list<File> &files) const;

		/**
		 * Checks if a file is the root of the file-system
		 * @return True, if the file is root.
		 */
		bool isRoot() const;

		/**
		 * Checks if a file is a system file like ".." and ".".
		 * @return True, if the file is a system file.
		 */
		bool isSystem() const;

		/**
		 * Checks if a file is a directory.
		 * @return True, if the file is a directory.
		 */
		bool isDirectory() const;

		/**
		 * @return True, if the object contains a valid path to a file, existing or not
		 */
		bool isValid() const;

		/**
		 * Returns the full path of the file (as given in the constructor).
		 * @return The path of the file.
		 */
		std::string getPath() const;

		/**
		 * Returns the basename of the file.
		 * @return Basename of the file.
		 */
		std::string getBasename() const;

		/**
		 * Remove a file.
		 * @param recursive If set to true, the deletion works recursive and deletes directories with files too.
		 * @return Returns zero on success and an error number on failure.
		 */
		virtual int remove(bool recursive = false);

		/**
		 * Get a specific file in this directory.
		 * @param filename The name of the file (not the full path).
		 * @return A file object of the contained file.
		 */
		File get(const std::string &filename) const;

		/**
		 * Get the parent of this file. This is always the containing directory.
		 * @return The parent directory of this file.
		 */
		File getParent() const;

		/**
		 * Checks whether this file exists or not.
		 * @return True, if the file exists.
		 */
		virtual bool exists() const;

		/**
		 * Updates file information like file type @see getType()
		 */
		virtual void update();

		/**
		 * Get the size of the file.
		 * @return The size in bytes.
		 */
		virtual size_t size() const;

		/**
		 * Get the timestamp of the last access
		 */
		virtual time_t lastaccess() const;

		/**
		 * Get the timestamp of the last modification
		 */
		virtual time_t lastmodify() const;

		/**
		 * Get the timestamp of the last status change
		 */
		virtual time_t laststatchange() const;

		/**
		 * This method creates a directory. This is done recursively.
		 * @param path The path to create.
		 */
		static void createDirectory(File &path);

		bool operator==(const ibrcommon::File &other) const;
		bool operator<(const ibrcommon::File &other) const;

	protected:
		File(const std::string &path, const unsigned char t);
		unsigned char _type;

	private:
		void resolveAbsolutePath();
		void removeSlash();
		std::string _path;
	};

	/**
	 * This exception is throw if a file not exists and a operation requires the file.
	 */
	class FileNotExistsException : public ibrcommon::IOException
	{
	public:
		FileNotExistsException(ibrcommon::File f) throw() : IOException("The file " + f.getPath() + " does not exists.")
		{
		};
	};

	/**
	 * This object creates a temporary filename. It does not create a file.
	 */
	class TemporaryFile : public File
	{
	public:
		TemporaryFile(const File &path, const std::string prefix = "file");
		virtual ~TemporaryFile();

	private:
		static std::string tmpname(const File &path, const std::string prefix);
	};
}

#endif /* FILE_H_ */
