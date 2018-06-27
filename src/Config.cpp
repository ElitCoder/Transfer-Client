#include "Config.h"
#include "Log.h"

#include <vector>
#include <fstream>

using namespace std;

void Config::add(const pair<string, deque<string>>& config) {
	configs_[config.first] = config.second;
}

static deque<string> getTokens(string input, char delimiter) {
	istringstream stream(input);
	deque<string> tokens;
	string token;
	
	while (getline(stream, token, delimiter))
		if (!token.empty())
			tokens.push_back(token);
	
	return tokens;
}

void Config::parse(const string& filename) {
	ifstream file(filename);
	
	if (!file.is_open()) {
		Log(WARNING) << "Could not open config " << filename << endl;
		
		return;
	}
	
	string line;
	
	while (getline(file, line)) {
		if (line.empty() || line.front() == '#')
			continue;
		
		auto tokens = getTokens(line, ' ');
		
		// Remove ':' from the setting
		tokens.front().pop_back();
		
		Log(DEBUG) << "Set key " << tokens.front() << " to value " << tokens.back() << endl;
		string key = tokens.front();
		tokens.pop_front();
		
		add({ key, tokens });
	}
	
	file.close();
}

void Config::clear() {
	configs_.clear();
}

unordered_map<string, deque<string>>& Config::internal() {
	return configs_;
}

bool Config::has(const string& key) {
	auto iterator = configs_.find(key);
	
	return iterator != configs_.end();
}