#include "InterpreterInterface.hpp"
#include <string>
#include "InterpreterImpl.hpp"
#include "Log.hpp"

namespace ELITE {

InterpreterPortInterface::Impl::Impl(PrimaryPortInterface& priamry) : primary_(priamry) {}

InterpreterPortInterface::Impl::~Impl() {
    
}

InterpreterPortInterface::InterpreterPortInterface(PrimaryPortInterface& primary) { impl_ = std::make_unique<Impl>(primary); }

bool InterpreterPortInterface::enterInterpreterMode(bool clear_queue_on_enter, bool clear_on_end) {
    std::string clear_queue_on_enter_str = clear_queue_on_enter ? "True" : "False";
    std::string clear_on_end_str = clear_on_end ? "True" : "False";
    std::string script = "def enterInterpreterMode():\n";
    script += "\tinterpreter_mode(" + clear_queue_on_enter_str + ", " + clear_on_end_str + ")\n";
    script += "end\n";
    ELITE_LOG_DEBUG("Enter interpreter script:\n%s", script.c_str());
    return impl_->primary_.sendScript(script);
}

bool InterpreterPortInterface::connect(const std::string& ip, int port, int timeout_ms) {
    impl_->io_context_ = std::make_unique<boost::asio::io_context>();
    impl_->work_guard_ptr_ = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
        boost::asio::make_work_guard(*impl_->io_context_));
    impl_->socket_ptr_ = std::make_unique<boost::asio::ip::tcp::socket>(*impl_->io_context_);
    impl_->socket_ptr_->open(boost::asio::ip::tcp::v4());
    impl_->socket_ptr_->set_option(boost::asio::ip::tcp::no_delay(true));
    impl_->socket_ptr_->set_option(boost::asio::socket_base::reuse_address(true));
    impl_->socket_ptr_->set_option(boost::asio::socket_base::keep_alive(false));
    impl_->socket_ptr_->non_blocking(true);
#if defined(__linux) || defined(linux) || defined(__linux__)
    impl_->socket_ptr_->set_option(boost::asio::detail::socket_option::boolean<IPPROTO_TCP, TCP_QUICKACK>(true));
#endif
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(ip), port);
    boost::system::error_code connect_ec;
    impl_->socket_ptr_->async_connect(endpoint, [&](const boost::system::error_code& ec) { connect_ec = ec; });
    impl_->io_context_->run_one_for(std::chrono::milliseconds(timeout_ms));
    if (connect_ec) {
        return false;
    }
    return true;
}



}  // namespace ELITE
