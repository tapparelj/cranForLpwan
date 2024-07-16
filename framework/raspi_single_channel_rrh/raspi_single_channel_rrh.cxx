#include "raspi_single_channel_rrh.hpp"
//#include "../config.hpp"
#include <boost/program_options.hpp>

namespace po = boost::program_options;

int main(int argc, char **argv)
{
    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message\nexample usage: ./raspi_single_channel_rrh --sf 7")
    ("sf", po::value<int>()->default_value(7), "Spreading factor the rrh will listen to")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 

    if (vm.count("help")) {
            std::cout << desc << "\n";
            return 1;
    }
    int sf = vm["sf"].as<int>();
    if(sf<7 || sf>12){
        std::cout << "Only spreading factors 7 to 12 are supported\n";
        std::cout << desc << "\n";
        return 1;
    }

    raspi_single_channel_rrh m_rrh = raspi_single_channel_rrh(sf);
    debug_print("---- Starting RRH -----","","rrh",YELLOW);
    m_rrh.tb->start();
    debug_print("---- RRH started -----","","rrh",YELLOW);
    m_rrh.tb->wait();
    debug_print("---- RRH finish wait -----","","rrh",YELLOW);
}
