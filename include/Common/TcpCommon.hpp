#ifndef __ELITE_TCP_COMMON_HPP__
#define __ELITE_TCP_COMMON_HPP__

#include <cstddef>
#include <string>

#if !defined(_WIN32)
#include <sys/socket.h>
#endif

namespace ELITE {

/**
 * @brief Status codes for socket I/O operations.
 *
 */
enum class SocketIOStatus {
    OK,
    TIMEOUT,
    CLOSED,
    ERROR
};

// Platform-specific socket handle type and invalid handle constant.
using SocketHandle =
#if defined(_WIN32)
    unsigned long long;
#else
    int;
#endif

#if defined(_WIN32)
// Platform-specific socket length type.
using SockLenType = int;
#else
// Platform-specific socket length type.
using SockLenType = socklen_t;
#endif

// Invalid socket handle constant.
static constexpr SocketHandle SOCKET_INVALID_HANDLE =
#if defined(_WIN32)
    static_cast<SocketHandle>(~0ULL);
#else
    -1;
#endif

/**
 * @brief Set socket options for a client socket.
 *
 * @param handle The socket handle.
 * @return true if the options were set successfully, false otherwise.
 */
bool socketClientSetOptions(SocketHandle handle);

/**
 * @brief Set the priority for a client socket.
 *
 * @param handle The socket handle.
 * @param priority The priority value.
 * @return true if the priority was set successfully, false otherwise.
 */
bool socketClientSetPriority(SocketHandle handle, int priority);

/**
 * @brief Set a socket to non-blocking mode.
 *
 * @param handle The socket handle.
 * @param non_blocking true to set non-blocking mode, false to set blocking mode.
 * @return true if the operation was successful, false otherwise.
 */
bool socketSetNonBlocking(SocketHandle handle, bool non_blocking);

/**
 * @brief Close a socket handle and reset it to invalid.
 *
 * @param handle The socket handle to close.
 */
void socketClose(SocketHandle& handle);

/**
 * @brief Get the last error code for the socket.
 *
 * @return int The last error code.
 */
int socketLastErrorCode();

/**
 * @brief Get the error string for a given error code.
 *
 * @param error_code The error code.
 * @return std::string The error string.
 */
std::string socketErrorString(int error_code);


/**
 * @brief Check if the error code indicates that the operation would block.
 *
 * @param error_code The error code.
 * @return true if the operation would block, false otherwise.
 */

bool socketWouldBlock(int error_code);

/**
 * @brief Wait until a socket becomes readable.
 *
 * @param handle The socket handle.
 * @param timeout_ms Timeout in milliseconds.
 * @return SocketIOStatus OK if readable, TIMEOUT if timed out, ERROR on failure.
 */
SocketIOStatus socketWaitReadable(SocketHandle handle, unsigned timeout_ms);

/**
 * @brief Wait until a socket becomes writable.
 *
 * @param handle The socket handle.
 * @param timeout_ms Timeout in milliseconds.
 * @return SocketIOStatus OK if writable, TIMEOUT if timed out, ERROR on failure.
 */
SocketIOStatus socketWaitWritable(SocketHandle handle, unsigned timeout_ms);

/**
 * @brief Receive data from a socket.
 *
 * @param handle The socket handle.
 * @param buff The buffer to store received data.
 * @param size The size of the buffer.
 * @param received_size The number of bytes actually received.
 * @return SocketIOStatus The status of the receive operation.
 */
SocketIOStatus socketReceive(SocketHandle handle, void* buff, std::size_t size, std::size_t& received_size);

/**
 * @brief Receive data from a socket.
 *
 * @param handle The socket handle.
 * @param buff The buffer to store received data.
 * @param size The size of the buffer.
 * @param received_size The number of bytes actually received.
 * @param timeout_ms Receive timeout in milliseconds.
 * @return SocketIOStatus The status of the receive operation.
 */
SocketIOStatus socketReceiveAll(SocketHandle handle, void* buff, std::size_t size, std::size_t& received_size,
                             int timeout_ms);

/**
 * @brief Receive a line of text from a socket.
 *
 * @param handle The socket handle.
 * @param line The string to store the received line.
 * @param timeout_ms Receive timeout in milliseconds.
 * @return SocketIOStatus The status of the receive operation.
 */
SocketIOStatus socketReceiveLine(SocketHandle handle, std::string& line, int timeout_ms);

/**
 * @brief Write data to a socket.
 *
 * @param handle The socket handle.
 * @param buff The buffer containing data to send.
 * @param size The size of the buffer.
 * @param sent_size The number of bytes actually sent.
 * @return SocketIOStatus The status of the send operation.
 */
SocketIOStatus socketWrite(SocketHandle handle, const void* buff, std::size_t size, std::size_t& sent_size);

/**
 * @brief Write all data to a socket, handling partial sends.
 *
 * @param handle The socket handle.
 * @param buff The buffer containing data to send.
 * @param size The size of the buffer.
 * @param sent_size The number of bytes successfully sent before returning.
 * @return SocketIOStatus The status of the send-all operation.
 */
SocketIOStatus socketWriteAll(SocketHandle handle, const void* buff, std::size_t size, std::size_t& sent_size);

}  // namespace ELITE

#endif  // __ELITE_TCP_COMMON_HPP__