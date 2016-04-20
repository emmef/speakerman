/*
 * SingleThreadFileCache.cpp
 *
 * Part of 'Speaker management system'
 *
 * Copyright (C) 2013-2014 Michel Fleur.
 * https://github.com/emmef/simpledsp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstring>
#include <cstdio>
#include <iostream>
#include <unistd.h>
#include <speakerman/SingleThreadFileCache.hpp>
#include <speakerman/SpeakermanConfig.hpp>
#include <speakerman/SpeakermanWebServer.hpp>
#include <tdap/Value.hpp>

namespace speakerman
{

using namespace tdap;

static string createFileName(const char *name)
{
	const char *path = getWebSiteDirectory();
	if (path) {
		string fullPath = path;
		fullPath += name;
		return fullPath;
	}
	string relativePath = "./";
	relativePath += name;
	return relativePath;
}

static void reallocate(char *&buffer, size_t &capacity, size_t size)
{
	if (buffer) {
		if (size <= capacity) {
			return;
		}
		char * newBuffer = new char[size];
		delete[] buffer;
		buffer = newBuffer;
		capacity = size;
	}
	buffer = new char[size];
	capacity = size;
}

class file_ptr_owner
{
	FILE * file_;
public:
	file_ptr_owner(FILE * file) : file_(file)
	{
	}

	void close()
	{
		if (file_) {
			fclose(file_);
			file_ = nullptr;
		}
	}

	operator FILE *() { return file_; }

	~file_ptr_owner()
	{
		close();
	}
};

file_entry::file_entry(const char* name) :
	name_(createFileName(name)), fileStamp_(0), lastChecked_(0)
{
}

void file_entry::reset()
{
	readPos_ = 0;
	long long now = current_millis();
	long long notCheckedFor = now - lastChecked_;
	if (notCheckedFor > -1000 && notCheckedFor < 1000) {
		return;
	}
	if (access(name_.c_str(), F_OK) != 0) {
		return;
	}
	long long fileStamp = getFileTimeStamp(name_.c_str());
	if (fileStamp == fileStamp_) {
		return;
	}
	std::cout << "I: Reading " << name_ << std::endl;
	file_ptr_owner f = fopen(name_.c_str(), "rb");
	if (fseek(f, 0, SEEK_END) != 0) {
		return ;
	}
	size_t size = ftell(f);
	if (fseek(f, 0, SEEK_SET) != 0) {
		return ;
	}
	reallocate(data_, capacity_, size);
	size_ = 0;
	size_t totalReads = 0;
	int attempt = 0;
	while (attempt < 10 && totalReads < size) {
		long long toRead = size - totalReads;
		if (toRead <= 0) {
			break;
		}
		long long reads = fread(data_ + totalReads, 1, toRead, f);
		if (reads > 0) {
			totalReads += reads;
		}
	}
	if (totalReads <= 0) {
		return;
	}
	size_ = totalReads;
	fileStamp_ = fileStamp;
}

signed long file_entry::read(void* buff, size_t offs, size_t length)
{
	if (!buff) {
		return stream_result::INVALID_ARGUMENT;
	}
	size_t maxReads = size_ - readPos_;
	size_t reads = Values::min(maxReads, length);
	memmove(static_cast<char *>(buff) + offs, data_ + readPos_, reads);
	readPos_ += reads;
	return reads;
}

int file_entry::read()
{
	if (readPos_ < size_) {
		return data_[readPos_++];
	}
	return stream_result::END_OF_STREAM;
}

void file_entry::close()
{
	readPos_ = 0;
}

file_entry::~file_entry()
{
	size_ = 0;
	capacity_ = 0;
	if (data_) {
		delete[] data_;
		data_ = nullptr;
	}
}


} /* End of namespace speakerman */
