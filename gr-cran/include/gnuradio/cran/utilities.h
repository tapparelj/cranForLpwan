#ifndef CRAN_UTILITIES_H
#define CRAN_UTILITIES_H

#include <cstdint>
#include <string.h>
#include <iomanip>
#include <numeric>
#include <gnuradio/expj.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <volk/volk.h>
#include <unordered_map>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if PRINT_DEBUG == true
#define debug_print(message,id,name,color) std::cout << color << "["<<name<<" "<<id<<"] "<< message << RESET << std::endl;

// #define debug_print_app(message) std::cout << "[Application] " << message << "\n"
// #define debug_print_buff(message) std::cout << GREEN << "[Buffer] " << message << RESET << "\n"
// #define debug_print_broker(message) std::cout << MAGENTA << "[Broker] " << message << RESET << "\n"
// #define debug_print_aggreg(message) std::cout << CYAN << "[Aggreg] " << message << RESET << "\n"
// #define debug_print_en(message) std::cout << BLUE << "[End Node ZMQ sink] " << message << RESET << "\n"
// #define debug_print_work(message) std::cout << BLUE << "[worker] " << message << RESET << "\n"
// #define debug_print_syncwork(message) std::cout << BLUE << "[sync_worker] " << message << RESET << "\n"
// #define debug_print_demodwork(message) std::cout << YELLOW << "[demod_worker] " << message << RESET << "\n"
// #define debug_print_edge_detect(message) std::cout << YELLOW << "[Frame Edge Detector] " << message << RESET << "\n"

#else
#define debug_print(message,id,name,color)
// #define debug_print_app(message) std::cout << "[Application] " << message << std::endl
// #define debug_print_buff(message)
// #define debug_print_broker(message)
// #define debug_print_aggreg(message)
// #define debug_print_en(message)
// #define debug_print_rrh(message)
// #define debug_print_work(message)
// #define debug_print_syncwork(message)
// #define debug_print_demodwork(message)
// #define debug_print_edge_detect(message)

#endif

#define error_print(message,id) std::cout << URED << "[ERROR] ["<<__FILE__<<" l."<<__LINE__<<" "<<id<<"] " << message << RESET << std::endl
#define warning_print(message,id) std::cout << UYELLOW << "[WARNING] ["<<__FILE__<<" l."<<__LINE__<<" "<<id<<"] " << message << RESET << std::endl

namespace gr
{
    namespace cran
    {

#define RESET "\033[0m"
#define WHITE "\033[0m"
#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define YELLOW "\033[1;33m"
#define BLUE "\033[1;34m"
#define MAGENTA "\033[1;35m"
#define CYAN "\033[1;36m"
#define GREY "\033[1;37m"
#define BROWN "\033[0;33m"
#define WHITE "\033[0m"

#define URED "\033[4;31m"  
#define UYELLOW "\033[4;33m"

#define SF_MIN 5
#define SF_MAX 12

        enum Header_type
        {
            NEW_FRAME,
            CONTINUE_FRAME,
            READY
        };
        struct Frame_info
        {
            Header_type frame_state;
            uint8_t channel_id;
            uint8_t sf;
            uint32_t samp_rate;
            uint32_t bandwidth;
            uint64_t time_full;
            double time_frac;
        };
        struct Work_info
        {
            uint16_t work_id;
            uint8_t channel_id;
            char demod_worker_addr[64];
            uint8_t sf;
            uint32_t samp_rate;
            uint32_t bandwidth;
            uint64_t time_full;
            double time_frac;
        };

#define CHANNEL_MASK 0xFFF0        // mask of the channel that gives the topic to which workers need to listen
        const std::vector<float> center_freq_MHz = {868.1, 868.3, 868.5, 867.1, 867.3, 867.5, 867.7, 867.9}; //<all possible channel center frequencies

        inline uint8_t get_channel_id(float fc) // fc in MHz
        {
            int fc_idx = 0;
            auto itr = std::find(center_freq_MHz.begin(), center_freq_MHz.end(), fc);
            if (itr != center_freq_MHz.end())
            {
                fc_idx = distance(center_freq_MHz.begin(), itr);
            }
            else
            {
                std::cerr << "Carrier frequency not in the list of channel supported." << std::endl;
            }
            return fc_idx;
        }

        inline float get_fc_from_channel(uint16_t channel)
        {
            return center_freq_MHz[(channel >> 3) & 0x07FF];
        }

        inline int get_fc_id_from_channel(uint16_t channel)
        {
            return (channel >> 3) & 0x07FF;
        }
        inline uint8_t get_sf_from_channel(uint16_t channel)
        {
            return (channel & 0x07) + SF_MIN;
        }

        inline int get_max_channels()
        {
            return (center_freq_MHz.size());
        }

        inline std::string get_ip(std::string interface)
        {
            struct ifaddrs *ifAddrStruct = NULL;
            struct ifaddrs *ifa = NULL;
            void *tmpAddrPtr = NULL;
            std::string ip_addr;
            getifaddrs(&ifAddrStruct);
            for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next)
            {
                if (ifa->ifa_addr->sa_family == AF_INET)
                { // check it is IP4
                    // is a valid IP4 Address
                    tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
                    char addressBuffer[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
                    if (strcmp(ifa->ifa_name, interface.c_str()) == 0)
                        ip_addr = addressBuffer;
                }
            }
            if (ifAddrStruct != NULL)
                freeifaddrs(ifAddrStruct); // remember to free ifAddrStruct
            return ip_addr;
        }

        inline uint32_t rev_endian(uint32_t number)
        {
            uint32_t val = ((number & 0xFF000000) >> 24) + ((number & 0xFF0000) >> 8) + ((number & 0xFF00) << 8) + ((number & 0xFF) << 24);
            return val;
        }

        /**
         *  \brief  Return an modulated upchirp using s_f=bw
         *
         *  \param  chirp
         *          The pointer to the modulated upchirp
         *  \param  id
         *          The number used to modulate the chirp
         * \param   sf
         *          The spreading factor to use
         * \param os_factor
         *          The oversampling factor used to generate the upchirp
         */
        inline void build_upchirp(gr_complex *chirp, uint32_t id, uint8_t sf, uint8_t os_factor = 1)
        {
            double N = (1 << sf);
            int n_fold = N * os_factor - id * os_factor;
            for (uint n = 0; n < N * os_factor; n++)
            {
                if (n < n_fold)
                    chirp[n] = gr_complex(1.0, 0.0) * gr_expj(2.0 * M_PI * (n * n / (2 * N) / pow(os_factor, 2) + (id / N - 0.5) * n / os_factor));
                else
                    chirp[n] = gr_complex(1.0, 0.0) * gr_expj(2.0 * M_PI * (n * n / (2 * N) / pow(os_factor, 2) + (id / N - 1.5) * n / os_factor));
            }
        }
        /**
         *  \brief  return the modulus a%b between 0 and (b-1)
         */
        inline long mod(long a, long b)
        {
            return (a % b + b) % b;
        }

    }
}
#endif /* CRAN_UTILITIES_H */
