#include "fileManager.hpp"
#include <iostream>

namespace fs = std::filesystem;

FileManager::FileManager()
{
	if (!fs::exists(root)) {
		fs::create_directory(root);
	}
}

FileManager::~FileManager() {
	if (dataFile.is_open()) {
		dataFile.close();
	}
}

bool FileManager::openTo(const std::filesystem::path& path)
{
	try {
		if (dataFile.is_open()) {
			dataFile.close();
		}
		dataFile.open(path, std::ios::out | std::ios::trunc);
	}
	catch (std::exception& e) {
		std::cout << e.what();
		return false;
	}
	return true;
}

void FileManager::close()
{
	dataFile.close();
}

bool FileManager::createDir(const std::filesystem::path& dir)
{
	return fs::create_directory(dir) ? true : false;
}

std::ostream& FileManager::getCurrentFileHandler()
{
	return dataFile;
}
