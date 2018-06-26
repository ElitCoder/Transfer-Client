#include "Parameter.h"
#include "Log.h"

using namespace std;

void Parameter::set(int argc, char** argv) {
	string option;
	vector<string> parameters;
	
	Log(DEBUG) << "Going through parameters\n";
	
	// Skip the program name
	for (int i = 1; i < argc; i++) {
		string parameter = argv[i];
		
		// Found an option
		if (parameter.front() == '-') {
			// Are there any parameters from an old option?
			if (!option.empty())
				set(option, parameters);
			
			option = parameter;
			parameters.clear();
			
			continue;
		}
		
		parameters.push_back(parameter);
	}
	
	if (!option.empty())
		set(option, parameters);
}

void Parameter::set(const string& option, const vector<string>& parameters) {
	options_[option] = parameters;
	
	Log(DEBUG) << "Set option " << option << " to ";
	
	for (auto& parameter : parameters)
		Log(NONE) << parameter << " ";
		
	Log(NONE) << endl;
}

const vector<string>& Parameter::get(const string& option) {
	auto iterator = options_.find(option);
	
	if (iterator == options_.end())
		throw exception();
		
	return iterator->second;
}

bool Parameter::has(const string& option) {
	try {
		get(option);
	} catch(...) {
		return false;
	}
	
	return true;
}