#pragma once
#include <fstream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

class FileManager {
protected:
	FileManager();
	~FileManager() = default;

	bool openTo(const std::string& path);

protected:
	std::ofstream dataFile;
	const std::filesystem::path root = fs::current_path().root_path().u8string() + "/targets/data";

private:
	virtual bool write() = 0; //only derived class version can be called
};