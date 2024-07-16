#include "application.hpp"
#include "../config.hpp" 
#include <boost/program_options.hpp>


// Influxdb default conf
#define HOST "192.168.122.1"                    // should be changed later to hostname resolved by gethostbyname, or 127.0.0.1 for final locally-run influxdb server
#define PORT 8086
#define BUCKET "test-buck"
#define MEAS "test records"              // measurements
#define USR "influxdb"
#define PASSWD "influxdb"
#define TOKEN "c3HpEEC6sSeSxdmjJdV0ptGrz1UJ8D_Ox9NWTAYOc_fW762BsEij-bibHNZfuxE5moFcLFMW_9ELd87TgjkONw=="

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
    ("savefile,s", po::value<std::string>()->default_value(""), "set savefile path")
    ("host-address,a", po::value<std::string>(&flux_host)->default_value(HOST), "set host address")
    ("port-number,p", po::value<uint16_t>(&flux_port)->default_value(PORT), "set port number")
    ("bucket,b", po::value<std::string>(&flux_bucket)->default_value(BUCKET), "set bucket name")
    ("measurements,m", po::value<std::string>(&flux_meas)->default_value(MEAS), "set measurement title")
    ("username,u", po::value<std::string>(&flux_usr)->default_value(USR), "set influxdb username")
    ("password,p", po::value<std::string>(&flux_passwd)->default_value(PASSWD), "set influxdb password")
    ("token,t", po::value<std::string>(&flux_token)->default_value(TOKEN), "set influxdb token");

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

    // set influxdb params before running app
    new_application.setFluxHost(flux_host);
    new_application.setFluxPort(flux_port);
    new_application.setFluxBucket(flux_bucket);
    new_application.setFluxMeas(flux_meas);
    new_application.setFluxUser(flux_usr);
    new_application.setFluxPasswd(flux_passwd);
    new_application.setFluxToken(flux_token);

    new_application.run();
}
