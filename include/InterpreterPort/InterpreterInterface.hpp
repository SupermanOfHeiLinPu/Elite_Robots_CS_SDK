#ifndef __ELITE_INTERPRETER_PORT_HPP__
#define __ELITE_INTERPRETER_PORT_HPP__

#include <memory>
#include <functional>
#include <string>
#include <Elite/PrimaryPortInterface.hpp>


namespace ELITE
{

class InterpreterPortInterface {
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
public:
    static constexpr int INTERPRETER_PORT = 30020;
    static constexpr int DEFAULT_TIMEOUT = 500;

    InterpreterPortInterface(PrimaryPortInterface& primary);

    ~InterpreterPortInterface() = default;

    bool enterInterpreterMode(bool clear_queue_on_enter, bool clear_on_end);

    bool connect(const std::string& ip, int port = INTERPRETER_PORT, int timeout_ms = DEFAULT_TIMEOUT);
};


} // namespace ELITE

#endif