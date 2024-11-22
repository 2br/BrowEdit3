#include "Util.h"
#include "FileIO.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>


namespace util
{
	std::vector<FileIO::Source*> FileIO::sources;

	void FileIO::begin()
	{
		for (auto& source : sources)
		{
			source->close();
			delete source;
		}
		sources.clear();
	}

	std::istream* FileIO::open(const std::string& fileName)
	{
		for (auto& source : sources)
		{
			if (source->exists(fileName))
				return source->open(fileName);
		}
		return nullptr;
	}
	std::string FileIO::getSrc(const std::string& fileName)
	{
		for (auto& source : sources)
		{
			if (source->exists(fileName))
				return source->toString();
		}
		return "No Src: " + fileName;
	}

	bool FileIO::exists(const std::string& fileName)
	{
		for (auto& source : sources)
			if (source->exists(fileName))
				return true;
		return false;
	}

	std::vector<std::string> FileIO::listFiles(const std::string& directory)
	{
		std::vector<std::string> files;
		for (auto& source : sources)
			source->listFiles(directory, files);
		return files;
	}

	std::vector<std::string> FileIO::listAllFiles()
	{
		std::vector<std::string> files;
		for (auto& source : sources)
			source->listAllFiles(files);
		return files;
	}


	void FileIO::addDirectory(const std::string& directory)
	{
		sources.push_back(new DirSource(directory));
	}

	void FileIO::addGrf(const std::string& grf)
	{
		sources.push_back(new GrfSource(grf));
	}


	void FileIO::end()
	{
		if (rootNode)
			delete rootNode;
		std::cout << "FileIO: building tree" << std::endl;
		rootNode = new Node("", nullptr);
		std::vector<std::string> files = FileIO::listAllFiles();

		std::map<std::string, Node*> cache;
		for (const auto& file : files)
		{
			auto isDir = file.rfind("\\");
			if(isDir == std::string::npos)
				rootNode->addFile(file);
			else
			{
				std::string dir = file.substr(0, isDir);
				auto cacheHit = cache.find(dir);
				if (cacheHit == cache.end())
					cache[dir] = rootNode->addFile(file);
				else
					cacheHit->second->addFile(file.substr(isDir+1));
			}
		}
		std::cout << "FileIO: done building tree" << std::endl;
	}

	void FileIO::reload(const std::string& path)
	{
		auto files = listFiles(path);
		std::map<std::string, Node*> cache;
		for (const auto& file : files)
		{
			auto isDir = file.rfind("\\");
			if (isDir == std::string::npos)
				rootNode->addFile(file);
			else
			{
				std::string dir = file.substr(0, isDir);
				auto cacheHit = cache.find(dir);
				if (cacheHit == cache.end())
					cache[dir] = rootNode->addFile(file);
				else
					cacheHit->second->addFile(file.substr(isDir + 1));
			}
		}
	}

	FileIO::Node* FileIO::Node::addFile(const std::string& fileName)
	{
		if (fileName.find("\\") == std::string::npos)
		{
			files.insert(iso_8859_1_to_utf8(fileName));
			return this;
		}
		else
		{
			std::string dir = fileName.substr(0, fileName.find("\\"));
			std::string rest = fileName.substr(fileName.find("\\")+1);
			auto dirNode = directories.find(dir);
			if (dirNode == directories.end())
			{
				directories[dir] = new Node(iso_8859_1_to_utf8(dir), this);
				dirNode = directories.find(dir);
			}
			return dirNode->second->addFile(rest);
		}
	}

	FileIO::Node* FileIO::Node::getDirectory(const std::string& directory)
	{
		if (directory == "")
			return this;
		std::string dir = directory;
		std::string rest = "";
		if (dir.find("\\") != std::string::npos)
		{
			dir = directory.substr(0, directory.find("\\"));
			rest = directory.substr(directory.find("\\") + 1);
		}
		if (directories.find(dir) == directories.end())
			return nullptr;
		return directories[dir]->getDirectory(rest);
	}

	FileIO::Node* FileIO::directoryNode(const std::string& directory)
	{
		return rootNode->getDirectory(directory);
	}

	////////GRF
	std::string FileIO::GrfSource::sanitizeFileName(std::string fileName)
	{
		std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);
		std::replace(fileName.begin(), fileName.end(), '/', '\\');

		std::size_t index;
		while ((index = fileName.find("\\\\")) != std::string::npos)
			fileName = fileName.substr(0, index) + fileName.substr(index + 1);
		return fileName;
	}


	FileIO::GrfSource::GrfSource(const std::string& fileName) : grfFile(fileName)
	{
		grfFileName = fileName;
		if (grfFileName.find("/") != std::string::npos)
			grfFileName = grfFileName.substr(grfFileName.rfind("/") + 1);
		if (grfFileName.find("\\") != std::string::npos)
			grfFileName = grfFileName.substr(grfFileName.rfind("\\") + 1);
		std::cout << "GRF: Loading " << fileName << std::endl;
		GrfError error;
		grf = grf_open(fileName.c_str(), "rb", &error);
		if (grf == NULL)
		{
			std::cerr << "Error opening GRF file: " << grfFile << std::endl;
			return;
		}
		for (unsigned int i = 0; i < grf->nfiles; i++)
			lookup[sanitizeFileName(grf->files[i].name)] = i;
		std::cout << "GRF: " << lookup.size() << " files loaded" << std::endl;
	}

	void FileIO::GrfSource::close()
	{
		grf_close(grf);
		grf = nullptr;
	}

	std::istream* FileIO::GrfSource::open(const std::string& fileName)
	{
		if (!grf)
			throw "error";
		auto it = lookup.find(sanitizeFileName(fileName));
		if (it == lookup.end())
			throw "error";

		GrfError error;
		unsigned int size = 0;
		char* data = (char*)grf_index_get(grf, it->second, &size, &error);
		auto ss = new std::istringstream(std::string(data, size));
		//todo: check if data should really not be freed
		return ss;
	}

	bool FileIO::GrfSource::exists(const std::string& fileName)
	{
		return lookup.find(sanitizeFileName(fileName)) != lookup.end();
	}

	void FileIO::GrfSource::listFiles(const std::string& directory, std::vector<std::string>& files)
	{
		for (auto kv : lookup)
		{
			auto filename = kv.first;
			if (filename.substr(0, directory.size()) == directory)
			{
				filename = filename.substr(directory.size());
				if (filename[0] == '\\')
					filename = filename.substr(1);
				if (filename.find("\\") == std::string::npos)
					if (std::find(files.begin(), files.end(), kv.first) == files.end())
						files.push_back(kv.first);
			}
		}
	}

	void FileIO::GrfSource::listAllFiles(std::vector<std::string>& files)
	{
		for (auto kv : lookup)
			files.push_back(kv.first);
	}

	std::string FileIO::GrfSource::toString()
	{
		return "Grf: " + this->grfFileName;
	}

	///////////////////////File
	FileIO::DirSource::DirSource(const std::string& dir) : directory(dir)
	{
		std::cout << "Using directory " << dir << std::endl;
	}
	void FileIO::DirSource::close()
	{
	}

	std::istream* FileIO::DirSource::open(const std::string& fileName)
	{
		return new std::ifstream(directory + fileName, std::ios_base::in | std::ios_base::binary);
	}

	bool FileIO::DirSource::exists(const std::string& fileName)
	{
		if (fileName == "")
			return false;
		std::error_code ec;
		try
		{
			bool exists = std::filesystem::exists(directory + fileName, ec); //TODO
			if (ec)
			{
				std::cerr << "Exception when checking if file exists: " << fileName << std::endl;
				std::cerr << ec << std::endl;
				std::cerr << ec.message() << std::endl;
			}
			return exists;
		}
		catch (...)
		{
			std::cerr << "Error checking if file exists" << std::endl;
			return false;
		}
	}

	void FileIO::DirSource::listFiles(const std::string& dir, std::vector<std::string>& files)
	{
		try {
			if (!std::filesystem::exists(directory + dir))
				return;
			for (const auto& entry : std::filesystem::directory_iterator(directory + dir))
			{
				auto f = entry.path().string();
				if (f.size() > directory.size() && directory.size() > 0 && f.find(directory) == 0)
					f = f.substr(directory.size());
				if (entry.is_directory() && f.find("game\\") != std::string::npos)
					continue;
				if (entry.is_directory())
					listFiles(f, files);
				else
					files.push_back(f);
			}
		}
		catch (...)
		{
			std::cerr << "Something went wrong with listing files" << std::endl;
		}
	}

	void FileIO::DirSource::listAllFiles(std::vector<std::string>& files)
	{
		listFiles("", files);
	}

	std::string FileIO::DirSource::toString()
	{
		return "Dir: " + this->directory;
	}

	std::string FileIO::readString(std::istream* is, int maxLength, int length)
	{
		char* buf = new char[maxLength];
		is->read(buf, maxLength);
		std::string ret(buf);
		if (length > -1)
			ret = std::string(buf, length);
		delete[] buf;
		return ret;
	}
	std::string FileIO::readStringDyn(std::istream* is)
	{
		int len;
		is->read(reinterpret_cast<char*>(&len), sizeof(int));
		if (len <= 0 || len > 1024)
		{
			std::cout << "Error offset " << is->tellg() << std::endl;
			throw std::exception("Error loading string");
		}
		char* buf = new char[len];
		is->read(buf, len);
		std::string ret(buf, len);
		delete[] buf;
		return ret;
	}

	void FileIO::writeString(std::ostream& os, const std::string& data, int length)
	{
		char* buf = new char[length];
		memset(buf, 0, length);
		memcpy(buf, data.c_str(), std::min(data.length(), (std::size_t)length-1));
		os.write(buf, length);
		delete[] buf;
	}

	nlohmann::json FileIO::getJson(const std::string& fileName)
	{
		nlohmann::json ret;
		auto file = open(fileName);
		if (file)
		{
			*file >> ret;
			delete file;
		}
		return ret;
	}
	std::string FileIO::getString(const std::string& fileName)
	{
		std::string ret = "";
		auto file = open(fileName);
		if (file)
		{
			file->seekg(0, std::ios_base::end);
			auto length = file->tellg();
			file->seekg(0, std::ios_base::beg);
			char* buf = new char[length];
			file->read(buf, length);
			ret = std::string(buf, length);
			delete[] buf;
			delete file;
		}
		return ret;
	}


}