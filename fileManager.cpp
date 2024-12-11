#include "fileManager.hpp"

FileManager::FileManager()
{
	if (!fs::exists(root)) {
		fs::create_directory(root.parent_path());
		fs::create_directory(root);
	}
}

bool FileManager::openTo(const std::string& path)
{
	try {
		if (this->dataFile.is_open()) {
			dataFile.close();
			dataFile.open(path, std::ios::out | std::ios::trunc);
		}
	}
	catch (std::exception& e) {
		return false;
	}
	return true;
}
