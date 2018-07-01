#include "IO.h"
#include "Log.h"

#include <sys/stat.h>
#include <fstream>
#include <dirent.h>

#ifndef WIN32
#include <curl/curl.h>	// Not for Windows right now
#endif

#ifdef WIN32
#include <direct.h>    // For _mkdir in Windows
#endif

using namespace std;

#ifndef WIN32
void IO::download(const string& url, const string& name) {
	CURL* curl = curl_easy_init();
	
	if (!curl) {
		Log(WARNING) << "curl could not be initialized, auto-update is not available\n";
		
		return;
	}
	
	FILE* file = fopen(name.c_str(), "w");
	
	if (!file) {
		Log(ERROR) << "Output file " << name << " could not be opened\n";
		
		curl_easy_cleanup(curl);
		return;
	}
	
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
#ifndef WIN32
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
#endif
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
	
	CURLcode result = curl_easy_perform(curl);
	
	if (result != CURLE_OK)
		Log(WARNING) << "Transfer failed\n";

	curl_easy_cleanup(curl);
	fclose(file);
}
#endif

bool IO::isDirectory(const string& path) {
	struct stat stats;
	
	if (stat(path.c_str(), &stats) != 0)		
		throw exception();
	
	return stats.st_mode & S_IFDIR;
}

size_t IO::getSize(const string& path) {
	ifstream file(path, ios_base::binary);
	
	if (!file) {
		Log(ERROR) << "The file " << path << " could not be opened\n";
		
		throw exception();
	}
	
	file.seekg(0, ios_base::end);
	size_t size = file.tellg();
	//file.seekg(0, ios_base::beg);
	
	file.close();
	
	return size;
}

vector<string> IO::listDirectory(const string& path) {
	DIR* dir;
	struct dirent* ent;
	
	if ((dir = opendir(path.c_str())) == NULL) {
		Log(WARNING) << "Could not list directory " << path << endl;
		
		return vector<string>();
	}
	
	vector<string> contents;
	
	while ((ent = readdir(dir)) != NULL)
		contents.push_back(ent->d_name);
		
	closedir(dir);
	
	return contents;
}

static vector<string> getTokens(string input, const string& delimiter) {
	size_t pos = 0;
	vector<string> tokens;
	
	while ((pos = input.find(delimiter)) != string::npos) {
	    auto token = input.substr(0, pos);
	    input.erase(0, pos + delimiter.length());
		
		tokens.push_back(token);
	}
	
	return tokens;
}

void IO::createDirectory(const string& path) {
	auto folders = getTokens(path, "/");
	string current_path = "";
	
	for (auto& folder : folders) {
		current_path += folder + "/";
		
		#ifdef WIN32
		_mkdir(current_path.c_str());
		#else
		mkdir(current_path.c_str(), 0755);
		#endif		
	}
}