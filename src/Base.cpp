#include "Base.h"
#include "Config.h"
#include "NetworkCommunication.h"
#include "CLI.h"

Config Base::settings_;
NetworkCommunication Base::network_;
CLI Base::cli_;

Config& Base::settings() {
	return settings_;
}

NetworkCommunication& Base::network() {
	return network_;
}

CLI& Base::cli() {
	return cli_;
}