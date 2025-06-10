#ifndef __ELITE_ROBOT_EXCEPTION_HPP__
#define __ELITE_ROBOT_EXCEPTION_HPP__

#include <Elite/EliteOptions.hpp>
#include <cstdint>
#include <string>
#include <memory>

#if (ELITE_SDK_COMPILE_STANDARD >= 17)
#include <variant>
#elif (ELITE_SDK_COMPILE_STANDARD == 14)
#include <boost/variant.hpp>
#endif

namespace ELITE {

class RobotException {
   public:
    enum class Type : int8_t {ERROR = 6, RUNTIME = 10 };

    Type getType() { return type_; }

    uint64_t getTimeStamp() { return timestamp_; }

    RobotException() = delete;

    RobotException(Type type, uint64_t ts) : type_(type), timestamp_(ts) {}

    ~RobotException() = default;

   protected:
    Type type_;
    uint64_t timestamp_;
};

class RobotError : public RobotException {
   public:

    enum class Source : int {
        SAFETY = 99,
        GUI = 103,
        CONTROLLER = 104,
        RTSI = 105,
        JOINT = 120,
        TOOL = 121,
        TP = 122,
        JOINT_FPGA = 200,
        TOOL_FPGA = 201
    };

    enum class DataType : uint32_t { NONE = 0, UNSIGNED = 1, SIGNED = 2, FLOAT = 3, HEX = 4, STRING = 5, JOINT = 6 };

    enum class Level : int { INFO, WARNING, ERROR, FATAL };
    
#if (ELITE_SDK_COMPILE_STANDARD >= 17)
    using Data = std::variant<uint32_t, int32_t, float, std::string>;
#elif (ELITE_SDK_COMPILE_STANDARD == 14)
    using Data = boost::variant<uint32_t, int32_t, float, std::string>;
#endif
    int getErrorCode() { return code_; }

    int getSubErrorCode() { return sub_code_; }

    Source getErrorSouce() { return error_source_; }

    Level getErrorLevel() { return error_level_; }

    DataType getErrorDataType() { return error_data_type_; }

    Data getData() { return data_; };

    RobotError() = delete;

    RobotError(uint64_t ts, int code, int sc, Source es, Level el, DataType et, Data data)
        : RobotException(RobotException::Type::ERROR, ts),
          code_(code),
          sub_code_(sc),
          error_source_(es),
          error_level_(el),
          error_data_type_(et),
          data_(data) {}

    ~RobotError() = default;

   private:
    int code_;

    int sub_code_;

    Source error_source_;

    Level error_level_;

    DataType error_data_type_;

    Data data_;
};

class RobotRuntimeException : public RobotException {
   private:
    int line_;
    int column_;
    std::string message_;

   public:
    int getLine() { return line_; }

    int getColumn() { return column_; }

    const std::string& getMessage() { return message_; }

    RobotRuntimeException() = delete;
    RobotRuntimeException(uint64_t ts, int line, int column, std::string&& msg)
        : RobotException(RobotException::Type::RUNTIME, ts), line_(line), column_(column), message_(std::move(msg)) {}
    ~RobotRuntimeException() = default;
};

using RobotExceptionSharedPtr = std::shared_ptr<RobotException>;
using RobotErrorSharedPtr = std::shared_ptr<RobotError>;
using RobotRuntimeExceptionSharedPtr = std::shared_ptr<RobotRuntimeException>;

}  // namespace ELITE

#endif