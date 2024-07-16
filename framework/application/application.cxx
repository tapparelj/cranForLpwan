#include "application.hpp"
#include "../config.hpp" 
#include <boost/program_options.hpp>


namespace po = boost::program_options;

int main(int argc, char** argv){
    std::string flux_host;          // flux conf variables; all set to default in ::add_options
    uint16_t flux_port;
    std::string flux_bucket;
    std::string flux_meas;
    std::string flux_usr;
    std::string flux_passwd;
    std::string flux_token;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message\nexample usage: ./application -s tmp.log")
    ("savefile,s", po::value<std::string>()->default_value(""), "set savefile path");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 

    if (vm.count("help")) {
            std::cout << desc << "\n";
            return 1;
    }

    // std::string arg1;
    if(vm["savefile"].as<std::string>()==""){
        std::cout<<RED<<"no savefile path given, wont save results. Use -s option to set savefile path"<<RESET<<std::endl;
    }
    application new_application = application(config.application_address,vm["savefile"].as<std::string>());

    new_application.run();
}
