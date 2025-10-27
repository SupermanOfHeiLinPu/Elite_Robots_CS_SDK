#include "Logger.hpp"

namespace ELITE{

Logger& getLogger() {
    static Logger* s_logger = new Logger();
    return *s_logger;
}

}
