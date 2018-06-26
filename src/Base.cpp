#include "Base.h"
#include "Config.h"
#include "NetworkCommunication.h"
#include "CLI.h"
#include "Parameter.h"

Config Base::config_;
NetworkCommunication Base::network_;
CLI Base::cli_;
Parameter Base::parameter_;

Config& Base::config() {
	return config_;
}

NetworkCommunication& Base::network() {
	return network_;
}

CLI& Base::cli() {
	return cli_;
}

Parameter& Base::parameter() {
	return parameter_;
}