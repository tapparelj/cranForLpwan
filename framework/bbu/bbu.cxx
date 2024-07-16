#include "broker.hpp"
#include "../config.hpp"
#include <boost/program_options.hpp>

namespace po = boost::program_options;

int main(int argc, char** argv){
    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message\nexample usage: ./bbu --saveDataFolder save_folder")
    ("saveDataFolder,s", po::value<std::string>()->default_value(""), "save folder for data received by buffer")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 

    if (vm.count("help")) {
            std::cout << desc << "\n";
            return 1;
    }
    if(vm["saveDataFolder"].as<std::string>() == "" ){
        std::cout<<"No save folder given, buffer data won't be saved\n";
    }
    std::cout<<"saving data to folder: "<<vm["saveDataFolder"].as<std::string>()<<"\n";

    broker new_broker =  broker(config.buffer_pull_addr_start, config.broker_front_rrh_addr, config.application_address,vm["saveDataFolder"].as<std::string>());
    new_broker.run();
}