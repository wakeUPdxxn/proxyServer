#pragma once
#include <fstream>
#include <filesystem>
#include <string>

class FileManager {
protected:
	FileManager();
	~FileManager();

	bool openTo(const std::filesystem::path&);
	bool createDir(const std::filesystem::path&);
	void close();

	template<typename _From,typename _To>
	auto addPath(const _From& from, const _To& to) const
	{
		return std::filesystem::path(from).concat("/").concat(to.c_str());
	}

	std::ostream& getCurrentFileHandler();

protected:
	const std::filesystem::path root = std::filesystem::current_path().root_path().concat("/targets"); 

private:
	std::ofstream dataFile;
	virtual bool write() = 0; //only derived class version can be called
};