#ifndef __ELITE_INTERPRETER_PORT_IMPL_HPP__
#define __ELITE_INTERPRETER_PORT_IMPL_HPP__

#include "InterpreterInterface.hpp"
#include "PrimaryPortInterface.hpp"

#include <boost/asio.hpp>
#include <thread>
#include <memory>

namespace ELITE {
class InterpreterPortInterface::Impl {
public:
    Impl(PrimaryPortInterface& priamry);
    ~Impl();

    std::unique_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_ptr_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_ptr_;

    PrimaryPortInterface& primary_;

};
}






#endif
