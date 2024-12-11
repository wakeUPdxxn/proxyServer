#pragma once
#include <fstream>
#include <filesystem>
#include <string>

class FileManager {
protected:
	FileManager();
	~FileManager();

	bool openTo(const std::filesystem::path& path);
	bool createDir(const std::filesystem::path& dir);
	void close();

protected:
	std::ofstream dataFile;
	const std::filesystem::path root = std::filesystem::current_path().root_path().concat("/targets/");

private:
	virtual bool write() = 0; //only derived class version can be called
};